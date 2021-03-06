#include <QDebug>

#include <QtCore>
#include <QDateTime>
#include <QTextDocument>
#include <QtConcurrent/QtConcurrent>
#include "evernotesession.h"
const std::string EvernoteSession::CONSUMER_KEY = "everel";
const std::string EvernoteSession::CONSUMER_SECRET = "201d20eb3ee1f74d";

EvernoteSession* EvernoteSession::m_instance = NULL;

EvernoteSession::EvernoteSession(QObject *parent) :
    QObject(parent)
{
    qDebug() << "EvernoteSession created" << endl;
    userStoreClient = NULL;
    syncClient = NULL;
    syncInProgress = false;
    syncCancelled = false;
    cancelGetNote = false;
}
EvernoteSession::~EvernoteSession() {
    if(userStoreClient){
        qDebug() << "EvernoteSession :: free UserStore client" << endl;
        delete userStoreClient;
    }

}

EvernoteSession* EvernoteSession::instance(){
    if(!m_instance){
        m_instance = new EvernoteSession();
    }
    return m_instance;
}

void EvernoteSession::drop(){
    if(m_instance){
        delete m_instance;
        m_instance = 0;
    }
}
void EvernoteSession::logout(){
    if(syncInProgress){
        return;
    }
    logoutStarted();
    cancelSync();
    DatabaseManager::instance()->clear();
    Cache::instance()->clear();
    Cache::instance()->clearFileCache();
    DatabaseManager::instance()->createTables();
    logoutFinished();
}
void EvernoteSession::logoutAsync(){
    if(syncInProgress){
        return;
    }
    QtConcurrent::run(this, &EvernoteSession::logout);
}

void EvernoteSession::exit(){
    qDebug() << "EvernoteSession :: exit" << endl;
    if(userStoreTransport){
        if(userStoreTransport->isOpen()){
            qDebug () << "EvernoteSession :: close UserStore transport... ";
            userStoreTransport->close();
            qDebug () << "closed" << endl;
        }else{
            qDebug() << "EvernoteSession :: UserStore transport already closed" << endl;
        }
    }

}

void EvernoteSession::recreateUserStoreClient(bool force){
    if(force){
        if(userStoreTransport != NULL){
            if(userStoreTransport->isOpen()){
                userStoreTransport->close();
            }
        }
        if(userStoreClient != NULL){
            delete userStoreClient;
            userStoreClient = NULL;
        }
    }
    if(userStoreClient == NULL){
        userStoreTransport = shared_ptr<TTransport> (new THttpClient(Constants::EDAM_HOST,80,Constants::EDAM_USER_ROOT));
        shared_ptr<TProtocol> protocol(new TBinaryProtocol(userStoreTransport));
        userStoreClient = new UserStoreClient(protocol);
    }
    if(!userStoreTransport->isOpen()){
        userStoreTransport->open();
    }
}
void EvernoteSession::recreateSyncClient(bool force){
    if(force){
        if(syncTransport != NULL){
            if(syncTransport->isOpen()){
                syncTransport->close();
            }

        }
        if(syncClient != NULL){
            delete syncClient;
            syncClient = NULL;
        }
    }
    if(syncClient == NULL){
        User user = Settings::instance()->getUser();
        qDebug() << QString::fromStdString(user.shardId);
        syncTransport = shared_ptr<TTransport> (new THttpClient(Constants::EDAM_HOST,80,Constants::EDAM_NOTE_ROOT+user.shardId));
        shared_ptr<TProtocol> protocol(new TBinaryProtocol(syncTransport));
        syncClient = new NoteStoreClient(protocol);
    }
    if(!syncTransport->isOpen()){
        syncTransport->open();
    }
}
void EvernoteSession::getNoteContent(NoteWrapper* note){
    qDebug() << "EvernoteSession :: auth" << endl;
    noteLoadStarted(note);
    try {
        note->note.tagGuids = DatabaseManager::instance()->getNoteTagGuids(note->note);
        note->note.resources = DatabaseManager::instance()->getNoteResources(note->note);


        if(!FileUtils::noteCached(note)){
            recreateSyncClient(false);
            std::string content = "";
            syncClient->getNoteContent(content, Settings::instance()->getAuthToken().toStdString(),note->getGuid());
            FileUtils::cacheNoteContent(note, QString::fromStdString(content));
        }
        if(cancelGetNote){
            return;
        }
        noteContentDownloaded(/*FileUtils::noteContentFilePath(note)*/note);
        sleep(1);
        for(int i=0;i<note->note.resources.size();i++){
            Resource r = note->note.resources.at(i);
            if(!FileUtils::resourceCached(r)){
                recreateSyncClient(false);
                syncClient->getResource(r, Settings::instance()->getAuthToken().toStdString(),r.guid, true, false, true, false);
                FileUtils::cacheResourceContent(r);
                r.data.bodyHash = ResourceWrapper::convertToHex(r.data.bodyHash).toStdString();
            }
            if(cancelGetNote){
                return;
            }
            ResourceWrapper* w = new ResourceWrapper(r);
            resourceDownloaded(w);
        }

        noteLoadFinished(note);
    } catch (TException &tx) {
        qDebug() << "EvernoteSession :: excetion while getNoteContent: " << tx.what();
        if(!cancelGetNote){
            noteLoadError(QString::fromLocal8Bit(tx.what()));
        }else{
            qDebug() << "note load canceled, supress errors";
        }
    }
}
void EvernoteSession::getNoteContentAsync(NoteWrapper* note){
    cancelGetNote = false;
    QtConcurrent::run(this, &EvernoteSession::getNoteContent, note);
}
void EvernoteSession::cancelGetNoteContent(){
    cancelGetNote = true;
    try{
        if(syncTransport != NULL){
            syncTransport->close();
        }
        qDebug() << "close transport";
    }catch(TException& e){
        qDebug() << "exception while closing transport: " << QString::fromLocal8Bit(e.what());
    }
}

void EvernoteSession::authAsync(const QString& username, const QString& password){
    QtConcurrent::run(this, &EvernoteSession::auth, username, password);
}

void EvernoteSession::auth(const QString& username, const QString& password){
    qDebug() << "EvernoteSession :: auth" << endl;
    try {
        recreateUserStoreClient(true);
        AuthenticationResult result;
        userStoreClient->authenticate(result,username.toStdString(),password.toStdString(),CONSUMER_KEY,CONSUMER_SECRET, 0);
        qDebug() << "EvernoteSession :: got auth token " << result.authenticationToken.c_str();
        Settings::instance()->setUsername(username);
        Settings::instance()->setPassword(password);
        Settings::instance()->setAuthToken(result.authenticationToken.c_str());
        Settings::instance()->setUser(result.user);
        recreateSyncClient(true);
        authenticationSuccess();
    }catch (EDAMUserException& e){
        if(e.errorCode == EDAMErrorCode::DATA_REQUIRED){
            if(e.parameter == "password"){
                authenticationFailed(tr("__empty_password__"));
            }else if(e.parameter == "username"){
                authenticationFailed(tr("__empty_username__"));
            }
        }else if(e.errorCode == EDAMErrorCode::INVALID_AUTH){
            if(e.parameter == "password"){
                authenticationFailed(tr("__invalid_password__"));
            }else if(e.parameter == "username"){
                authenticationFailed(tr("__invalid_username__"));
            }
        }else{
            authenticationFailed(tr("__basic_network_error__"));
        }

    }

    catch (TException &tx) {
        qDebug() << "EvernoteSession :: excetion while login: " << tx.what();
        authenticationFailed(tr("__basic_network_error__"));
    }
}
void EvernoteSession::reauth(){
    auth(Settings::instance()->getUsername(), Settings::instance()->getPassword());
}

void EvernoteSession::sync(){
    if(syncInProgress){
        return;
    }
    syncInProgress = true;
    syncCancelled = false;

    try{
        for(int i=0;i<5;i++){
            try{

                recreateUserStoreClient(false);
                recreateSyncClient(false);

                qDebug() << "EvernoteSession :: start sync...";
                int cacheUsn = DatabaseManager::instance()->getIntSetting(SettingsKeys::SERVER_USN);
                qDebug() << "EvernoteSession :: saved USN: " << cacheUsn;
                SyncChunk chunk;
                int percent = 0;
                while(true){
                    syncStarted(percent);
                    syncClient->getSyncChunk(chunk, Settings::instance()->getAuthToken().toStdString(), cacheUsn, 1024, false);

                    if(cacheUsn >= chunk.updateCount){
                        break;
                    }
                    percent = (int)((double)(100* (double)cacheUsn/(double)chunk.updateCount));
                    syncStarted(percent);
                    std::vector <Tag> tags = chunk.tags;

                    if(!tags.empty()){


                        tagsSyncStarted();
                        DatabaseManager::instance()->beginTransacton();
                        for(int i=0;i<tags.size();i++){
                            if(syncCancelled){
                                syncCancelled = false;
                                syncInProgress = false;
                                syncFinished();
                                return;
                            }
                            Tag tag = tags.at(i);
                            DatabaseManager::instance()->saveTag(tag);
                            qDebug() << "EvernoteSession :: tag " << tag.name.c_str();
                        }
                        DatabaseManager::instance()->commitTransaction();
                    }
                    syncStarted(percent);
                    if(syncCancelled){
                        syncCancelled = false;
                        syncInProgress = false;
                        syncFinished();
                        return;
                    }

                    std::vector <Notebook> notebooks = chunk.notebooks;
                    qDebug() << "EvernoteSession :: notebooks " << chunk.notebooks.size();
                    if(!notebooks.empty()){


                        notebooksSyncStarted();
                        DatabaseManager::instance()->beginTransacton();
                        for(int i=0;i<notebooks.size();i++){
                            if(syncCancelled){
                                syncCancelled = false;
                                syncInProgress = false;
                                syncFinished();
                                return;
                            }
                            Notebook notebook = notebooks.at(i);
                            DatabaseManager::instance()->saveNotebook(notebook);
                            qDebug() << "EvernoteSession :: notebook " << notebook.name.c_str();
                        }
                        DatabaseManager::instance()->commitTransaction();
                    }
                    syncStarted(percent);
                    if(syncCancelled){
                        syncCancelled = false;
                        syncInProgress = false;
                        syncFinished();
                        return;
                    }
                    std::vector <Note> notes = chunk.notes;
                    qDebug() << "EvernoteSession :: notes " << chunk.notes.size();
                    if(!notes.empty()){
                        DatabaseManager::instance()->beginTransacton();
                        for(int i=0;i<notes.size();i++){
                            if(syncCancelled){
                                syncCancelled = false;
                                syncInProgress = false;
                                syncFinished();
                                return;
                            }
                            Note note = notes.at(i);
                            if(note.deleted){
                                DatabaseManager::instance()->deleteNote(note);
                            }else{
                                DatabaseManager::instance()->saveNote(note);
                            }
                            qDebug() << "EvernoteSession :: note " << note.title.c_str();
                        }
                        DatabaseManager::instance()->commitTransaction();
                    }
                    syncStarted(percent);
                    std::vector <SavedSearch> searches = chunk.searches;
                    qDebug() << "EvernoteSession :: searches " << chunk.searches.size();
                    if(!searches.empty()) {
                        DatabaseManager::instance()->beginTransacton();
                        for(int i=0; i < searches.size(); i++) {
                            SavedSearch search = searches.at(i);
                            DatabaseManager::instance()->saveSavedSearch(search);
                            qDebug() << "EvernoteSession :: search " << search.name.c_str();
                        }
                        DatabaseManager::instance()->commitTransaction();
                    }
                    syncStarted(percent);

                    qDebug() << "expunged notes: " << chunk.expungedNotes.size();

                    cacheUsn = chunk.chunkHighUSN;
                    DatabaseManager::instance()->beginTransacton();
                    DatabaseManager::instance()->makeIntSetting(SettingsKeys::SERVER_USN, cacheUsn);
                    DatabaseManager::instance()->commitTransaction();
                    if(cacheUsn >= chunk.updateCount){
                        break;
                    }
                    qDebug() << "Current usn: " << cacheUsn << " high usn: " << chunk.chunkHighUSN << ", update count: " << chunk.updateCount;
                }

                qDebug() << "EvernoteSession :: sync finished";
                break;
            }catch(EDAMSystemException &e){
                qDebug() << "EvernoteSession :: Edam SYSTEM EXCEPTION " << e.what() << " " << e.errorCode;
                if(e.errorCode == 9){
                    reauth();
                }
            }

        }
    }catch(TException &tx){
        qDebug() << "EvernoteSession :: excetion while sync: " << tx.what();
        syncFailed("Network error");
    }
    syncInProgress = false;
    syncFinished();
    Cache::instance()->load();
}

void EvernoteSession::syncAsync(){
    qDebug() << "syncAsync called";
    QtConcurrent::run(this, &EvernoteSession::sync);
}
bool EvernoteSession::isSyncInProgress(){
    return syncInProgress;
}
void EvernoteSession::cancelSync(){
    syncCancelled = true;
}

void EvernoteSession::addNote(NoteWrapper *note) {
    Note returned;
    try {
        recreateSyncClient(true);
        Note reference_note;
        reference_note.__isset.title = true;
        reference_note.title = note->note.title;
        reference_note.__isset.content = true;
        reference_note.__isset.contentHash = true;
        reference_note.__isset.contentLength = true;
        reference_note.__isset.notebookGuid = true;
        reference_note.notebookGuid = note->note.notebookGuid;
        std::string assembled_content = note->note.content;
        reference_note.content = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><!DOCTYPE en-note SYSTEM \"http://xml.evernote.com/pub/enml2.dtd\"><en-note>" + QString::fromStdString(assembled_content).replace("\n", "<br />").toStdString() + "</en-note>";
        reference_note.contentLength = assembled_content.size();
        QCryptographicHash hash( QCryptographicHash::Md5 );
        hash.addData(reference_note.content.c_str(), reference_note.contentLength);
        reference_note.contentHash = "..................";
        const char * hash_data = hash.result().data();
        memcpy( const_cast<char *>(reference_note.contentHash.c_str()), hash_data, 16);
        syncClient->createNote(returned, Settings::instance()->getAuthToken().toStdString(), reference_note);
    }
    catch(EDAMUserException &tx) {
        qDebug() << "EvernoteSession :: exception while adding note eue: " << tx.what() << " error code: " << tx.errorCode;
        qDebug() << " parameter " << QString::fromStdString(tx.parameter);
    }
}

void EvernoteSession::updateNote(NoteWrapper *note) {
    Note ret;
    try {
        recreateSyncClient(false);
        Note reference_note;
        NoteAttributes attrs;

        if (note->getReminderDate().toMSecsSinceEpoch() > 0) {
            attrs.reminderTime = note->note.attributes.reminderTime;
            attrs.__isset.reminderTime = true;
            reference_note.attributes = attrs;
            reference_note.__isset.attributes = true;
        }
        reference_note.__isset.title = true;
        reference_note.__isset.content = true;
        reference_note.__isset.guid = true;
        reference_note.__isset.notebookGuid = true;
        reference_note.title = note->note.title;
        reference_note.guid = note->note.guid;
        reference_note.notebookGuid = note->note.notebookGuid;
        QTextDocument doc;
        doc.setHtml(QString::fromStdString(note->note.content));
        std::string assembled_content = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><!DOCTYPE en-note SYSTEM \"http://xml.evernote.com/pub/enml2.dtd\"><en-note>" + doc.toPlainText().replace("\n","<br />").toStdString() + "</en-note>";
        qDebug() << QString::fromStdString(assembled_content);
        reference_note.content = assembled_content;
        syncClient->updateNote(ret, Settings::instance()->getAuthToken().toStdString(), reference_note);
    } catch(EDAMUserException &tx) {
        qDebug() << "EvernoteSession :: update failed: " << tx.what() << " error code: " << tx.errorCode;
        qDebug() << " parameter " << QString::fromStdString(tx.parameter);
    }
}

void EvernoteSession::updateNoteTags(NoteWrapper *note) {
    Note ret;
    try {
        recreateSyncClient(false);
        Note reference_note;
        reference_note.__isset.tagGuids = true;
        reference_note.__isset.guid = true;
        reference_note.__isset.title = true;
        reference_note.title = note->note.title;
        reference_note.guid = note->note.guid;
        reference_note.tagGuids = note->note.tagGuids;
        syncClient->updateNote(ret, Settings::instance()->getAuthToken().toStdString(), reference_note);
    } catch(EDAMUserException &tx) {
        qDebug() << "EvernoteSession :: update tags failed: " << tx.what() << " error code: " << tx.errorCode;
        qDebug() << " parameter " << QString::fromStdString(tx.parameter);
    }
}

QString EvernoteSession::createTag(QString name) {
    try {
        Tag reference_tag;
        reference_tag.name = name.toStdString();
        reference_tag.__isset.name = true;
        syncClient->createTag(reference_tag, Settings::instance()->getAuthToken().toStdString(), reference_tag);
        return QString::fromStdString(reference_tag.guid);
    } catch(EDAMUserException &tx) {
        qDebug() << "EvernoteSession :: update tags failed: " << tx.what() << " error code: " << tx.errorCode;
        qDebug() << " parameter " << QString::fromStdString(tx.parameter);
    }
}

void EvernoteSession::deleteNote(NoteWrapper *note) {
    try {
        syncClient->deleteNote(Settings::instance()->getAuthToken().toStdString(), note->getGuid());
        EvernoteSession::instance()->syncAsync();
    } catch(TException &tx) {
        qDebug() << "EvernoteSession :: delete failed" << tx.what();
    }
}

void EvernoteSession::getNoteTags(NoteWrapper *note)
{
    note->note.tagGuids = DatabaseManager::instance()->getNoteTagGuids(note->note);
}

void EvernoteSession::searchNotes(QString query)
{
    try {
        NotesMetadataList result;
        NoteFilter filter;
        NotesMetadataResultSpec spec;
        spec.__isset.includeCreated = true;
        spec.__isset.includeTagGuids = true;
        spec.__isset.includeTitle = true;
        spec.includeTitle = true;
        spec.includeCreated = true;
        spec.includeTagGuids = true;
        query = query + "*";
        filter.words = query.toStdString();
        filter.__isset.words = true;
        filter.ascending = true;
        filter.__isset.ascending = true;
        syncClient->findNotesMetadata(result, Settings::instance()->getAuthToken().toStdString(), filter, 0, 10, spec);
        qDebug() << result.totalNotes;
        if (result.totalNotes > 0) {
            Cache::instance()->fireClearNotes();
            for (int i = 0; i < result.notes.size(); i++){
                Cache::instance()->fireNoteAdded(Cache::instance()->getNoteForGuid(QString::fromStdString(result.notes.at(i).guid)));
            }
        }

    } catch(TException &tx) {
            qDebug() << "EvernoteSession :: search failed" << tx.what();
    }
}

void EvernoteSession::addSavedSearch(SavedSearchWrapper *search) {
    try {
        SavedSearch ref_search;
        ref_search.__isset.name = true;
        ref_search.__isset.query = true;
        ref_search.name = search->getName().toStdString();
        ref_search.query = search->getQuery().toStdString();
        syncClient->createSearch(ref_search, Settings::instance()->getAuthToken().toStdString(), ref_search);
    } catch(EDAMUserException &tx) {
        qDebug() << "EvernoteSession :: create search failed: " << tx.what() << " error code: " << tx.errorCode << " parameter " << QString::fromStdString(tx.parameter);
    }
}

void EvernoteSession::updateSavedSearch(SavedSearchWrapper *search) {
    try {
        SavedSearch ref_search;
        ref_search.__isset.name = true;
        ref_search.__isset.query = true;
        ref_search.__isset.guid = true;
        ref_search.name = search->getName().toStdString();
        ref_search.query = search->getQuery().toStdString();
        ref_search.guid = search->getGuid().toStdString();
        syncClient->updateSearch(Settings::instance()->getAuthToken().toStdString(), ref_search);
    } catch(EDAMUserException &tx) {
        qDebug() << "EvernoteSession :: update search failed: " << tx.what() << " error code: " << tx.errorCode << " parameter " << QString::fromStdString(tx.parameter);
    }
}

void EvernoteSession::addNotebook(NotebookWrapper *notebook){
    try {
        Notebook ref_notebook;
        ref_notebook.__isset.name = true;
        ref_notebook.name = notebook->getName().toStdString();
        syncClient->createNotebook(ref_notebook, Settings::instance()->getAuthToken().toStdString(), ref_notebook);
    } catch(EDAMUserException &tx) {
        qDebug() << "EvernoteSession :: update search failed: " << tx.what() << " error code: " << tx.errorCode << " parameter " << QString::fromStdString(tx.parameter);
    }
}
