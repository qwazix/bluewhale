#include "tagwrapper.h"

TagWrapper::TagWrapper(QObject *parent) :
    QObject(parent)
{

}

TagWrapper::TagWrapper(Tag tag, QObject *parent):QObject(parent){
    this->tag = tag;
}
QString TagWrapper::getName(){
    return QString::fromStdString(tag.name);
}
QString TagWrapper::getGuid(){
    return QString::fromStdString(tag.guid);
}
void TagWrapper::setGuid(QString guid){
    this->tag.guid = guid.toStdString();
}
void TagWrapper::setName(QString name){
    this->tag.name = name.toStdString();
}
