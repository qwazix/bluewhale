#include "cache.h"
#include <QDebug>
#include <QUuid>
#include <QtCore>
Cache* Cache::m_instance = NULL;
Notebook* Cache::m_sel_notebook = NULL;

Cache::Cache(QObject *parent) :
    QObject(parent),
    nextNotebook(0)
{
    tags = new QVector <Tag> ();
    notebooks = new QVector <Notebook> ();
    notes = new QVector <Note>();
}

Cache* Cache::instance(){
   if(!m_instance){
        m_instance = new Cache();
   }
   return m_instance;
}
void Cache::setNextNotebook()
{
    m_sel_notebook = const_cast<Notebook*>(&notebooks->at(nextNotebook));
    notebookFired(new NotebookWrapper(notebooks->at(nextNotebook)));
    nextNotebook++;
    if (nextNotebook >= notebooks->size()) {
        nextNotebook = 0;
    }

}

Notebook* Cache::selectedNotebook(){
    if (!m_sel_notebook) {
        for (int i=0; i < Cache::instance()->notebooks->size(); i++) {
            Notebook notebook = Cache::instance()->notebooks->at(i);
            if(notebook.defaultNotebook) {
                qDebug() << "Got notebook " << QString::fromStdString(notebook.name);
                m_sel_notebook = const_cast<Notebook*>(&Cache::instance()->notebooks->at(i));
            }
        }
    }
    return m_sel_notebook;
}

void Cache::setSelectedNotebook(QString guid){
    for (int i=0; i < notebooks->size(); i++) {
        Notebook notebook = notebooks->at(i);
        if(notebook.guid == guid.toStdString()) {
            qDebug() << "Got notebook " << QString::fromStdString(notebook.guid);
            m_sel_notebook = const_cast<Notebook*>(&notebooks->at(i));
        }
    }
}

Cache::~Cache() {

}

void Cache::clear(){
    tags->clear();
    notebooks->clear();
    notes->clear();

}

void Cache::softLoad() {
    clearNotes();
    clearSearches();
    qDebug() << "Notes size: " << notes->size();

    for(int i=0;i<notes->size();i++){
        Note note = notes->at(i);
        if (m_sel_notebook) {
            if (note.notebookGuid == m_sel_notebook->guid){
                NoteWrapper* noteWrapper = new NoteWrapper(note);
                noteAdded(noteWrapper);
            }
        } else {
            NoteWrapper* noteWrapper = new NoteWrapper(note);
            noteAdded(noteWrapper);
        }
    }
}

void Cache::load(){
    tags = DatabaseManager::instance()->getTags();
    notebooks = DatabaseManager::instance()->getNotebooks();
    notes = DatabaseManager::instance()->getNotes();
    searches = DatabaseManager::instance()->getSavedSearches();
    clearNotes();
    clearSearches();
    qDebug() << "Notes size: " << notes->size();

    for(int i=0;i<notes->size();i++){
        Note note = notes->at(i);
        if (m_sel_notebook) {
            if (note.notebookGuid == m_sel_notebook->guid){
                NoteWrapper* noteWrapper = new NoteWrapper(note);
                noteAdded(noteWrapper);
            }
        } else {
            NoteWrapper* noteWrapper = new NoteWrapper(note);
            noteAdded(noteWrapper);
        }
    }
}
NoteWrapper* Cache::getNote(int index){
    if (!notes->isEmpty()) {
        return new NoteWrapper(notes->at(index));
    } else {
        return NULL;
    }
}
QString Cache::getNoteContent(NoteWrapper* note){
    return FileUtils::readNoteContent(note);
}

void Cache::clearFileCache(){
    FileUtils::removeNoteDir();
}

void Cache::fillWithTags(){
    for(int i=0;i<tags->size();i++){
        Tag tag = tags->at(i);
        TagWrapper* wrapper = new TagWrapper(tag);
        tagFired(wrapper);
    }
}
void Cache::fillWithNotebooks(){
    for(int i=0;i<notebooks->size();i++){
        Notebook notebook = notebooks->at(i);
        if (notebook.defaultNotebook) {
            m_sel_notebook = &notebook;
        }
        NotebookWrapper* wrapper = new NotebookWrapper(notebook);
        notebookFired(wrapper);
    }
}
void Cache::fillWithSavedSearches() {
    for(int i=0; i < searches->size(); i++) {
        SavedSearch search = searches->at(i);
        SavedSearchWrapper* wrapper = new SavedSearchWrapper(search);
        savedSearchFired(wrapper);
    }
}

QString Cache::getCacheFileName(NoteWrapper* note){
    return FileUtils::noteContentFilePath(note);
}
NotebookWrapper* Cache::getNotebook(NoteWrapper* note){
    for(int i=0;i<notebooks->size();i++){
        Notebook notebook = notebooks->at(i);
        if(notebook.guid.compare(note->getNotebookGUID().toStdString()) == 0){
            return new NotebookWrapper(notebook);
        }
    }
}
NotebookWrapper* Cache::getFirstNoteBook() {
    return new NotebookWrapper(notebooks->at(0));
}
TagWrapper* Cache::getTagForGuid(QString guid) {
    for(int i=0; i<tags->size(); i++) {
        if(tags->at(i).guid == guid.toStdString()) {
            return new TagWrapper(tags->at(i));
        }
    }
}
NoteWrapper* Cache::getNoteForGuid(QString guid) {
    for (int i=0; i < notes->size(); i++) {
        if(notes->at(i).guid == guid.toStdString()) {
            return new NoteWrapper(notes->at(i));
        }
    }
}
SavedSearchWrapper* Cache::getSavedSearch(int index) {
    return new SavedSearchWrapper(searches->at(index));
}
SavedSearchWrapper* Cache::getSavedSearchForGuid(QString guid) {
    for (int i=0; i < searches->size(); i++) {
        if(searches->at(i).guid == guid.toStdString()) {
            return new SavedSearchWrapper(searches->at(i));
        }
    }
}

QString Cache::genGuid() {
    return QUuid::createUuid().toString().replace("{","").replace("}","");
}

ResourceWrapper* Cache::getResourceForNote(NoteWrapper* note, QString guid){
    std::vector<Resource> resources = DatabaseManager::instance()->getNoteResources(note->note);
    for (int i = 0; i < resources.size(); i++) {
        Resource res = resources.at(i);
        if (res.guid == guid.toStdString()) {
            return new ResourceWrapper(res);
        }
    }
}

void Cache::setToDefaultNotebook()
{
    m_sel_notebook = NULL;
}

void Cache::fireClearNotes() {
    clearNotes();
}

void Cache::fireNoteAdded(NoteWrapper *note) {
    noteAdded(note);
}

void Cache::fireClearSearches() {
    clearSearches();
}

/*void Cache::loadTags(){
    if(isTagsLoaded() || EvernoteSession::instance()->isSyncInProgress()){
       return;
    }
    clearTags();
    tags = DatabaseManager::instance()->getTags();
    for(int i=0;i<tags->size();i++){
        Tag tag = tags->at(i);
        TagWrapper* tagWrapper = new TagWrapper(tag);
        tagAdded(tagWrapper);
    }
    setTagsLoaded(true);
}*/

