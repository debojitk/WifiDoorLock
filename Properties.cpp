/*
 * Properties.cpp
 *
 *  Created on: Sep 11, 2016
 *      Author: debojitk
 */

#include "Properties.h"
#include "debugmacros.h"
#include <stddef.h>
#include <Arduino.h>
#include <FS.h>
Properties::Properties(int size) {
	// TODO Auto-generated constructor stub
	_size=size;
	_currentSize=0;
	_size=size;
	_head=NULL;
	_tail=NULL;
}

Properties::~Properties() {
	// TODO Auto-generated destructor stub
}

void Properties::load(String fileName){
	File f = SPIFFS.open(fileName, "r");
	if (!f) {
		DEBUG_PRINT("file open failed->");DEBUG_PRINTLN(fileName);
		return;
	}else{
		DEBUG_PRINTLN("Loading properties from file");
		removeAll();
		while(f.available()){
			String line=f.readStringUntil('\r');f.read();//skipping \n
			if(line.startsWith("#")){
				continue;
			}
			int pos=line.indexOf('=');
			if(pos!=-1 && pos<line.length()){
				String key=line.substring(0,pos);
				String value=line.substring(pos+1);
				put(key, value);
			}
		}
		f.close();
	}

}
void Properties::store(String fileName){
	File f = SPIFFS.open(fileName, "w");
	KeyValuePair pair;
	if (!f) {
		DEBUG_PRINT("file open failed->");DEBUG_PRINTLN(fileName);
		return;
	}else{
		KeyValuePair *temp=_head;
		while(temp!=NULL){
			String line=temp->key+"="+temp->value;
			f.println(line);
			temp=temp->next;
		}
		f.close();
	}
}

String Properties::serialize(){
	String retVal="";
	KeyValuePair *temp=_head;
	while(temp!=NULL){
		String line=temp->key+"="+temp->value;
		retVal=retVal+","+line;
		temp=temp->next;
	}
	return retVal;
}

String Properties::get(String key){
	String retVal="";
	KeyValuePair *temp=_head;
	while(temp!=NULL){
		if(temp->key.equals(key)){
			retVal = temp->value;
			break;
		}
		temp=temp->next;
	}
	return retVal;
}
void Properties::put(String key, String value){
	KeyValuePair *kv=new KeyValuePair();
	kv->key=key;
	kv->value=value;
	if(_head==NULL){
		_head=_tail=kv;
	}else{
		KeyValuePair *temp=_head;
		while(temp!=NULL){
			if(temp->key.equals(key)){
				temp->value=value;
				delete(kv);
				return;
			}
			temp=temp->next;
		}
		_tail->next=kv;
		_tail=kv;
	}
	_currentSize++;
}
String Properties::remove(String key){
	String retVal="";
	KeyValuePair * parent=_head;
	KeyValuePair * node=_head;
	boolean found=false;
	while(node!=NULL){
		if(node->key.equals(key)){
			found=true;
			break;
		}
		parent=node;
		node=node->next;
	}
	if(found){
		retVal=node->value;
		//case 1
		if(node==_head){//delete first node
			if(_head==_tail){//one node only
				_head=_tail=NULL;
				delete(node);
			}else{// 1+ nodes
				_head=_head->next;
				delete(node);
			}
		}else{//1+ nodes
			parent->next=node->next;
			node->next=NULL;
			delete(node);
		}
		node=NULL;
		_currentSize--;
	}
	return retVal;
}

void Properties::parsePropertiesAndPut(String inputStr){
	int curPos=0;
	int nextPos=0;
	while((nextPos=inputStr.indexOf(',',curPos))!=-1){
		String pair=inputStr.substring(curPos,nextPos);
		int equalPos=pair.indexOf('=');
		if(equalPos!=-1 && equalPos<pair.length()-1){
			String key=pair.substring(0,equalPos);
			String value=pair.substring(equalPos+1);
			put(key,value);
		}
		curPos=nextPos+1;//advancing
	}
	if(curPos<inputStr.length()){//not ending with , so this is the last entry
		String pair=inputStr.substring(curPos);
		int equalPos=pair.indexOf('=');
		if(equalPos!=-1 && equalPos<pair.length()-1){
			String key=pair.substring(0,equalPos);
			String value=pair.substring(equalPos+1);
			put(key,value);
		}
	}
}

void Properties::removeAll(){
	KeyValuePair *temp=_head;
	KeyValuePair *prev=_head;
	while(temp!=NULL){
		prev=temp;
		temp=temp->next;
		delete(prev);
	}
	_head=_tail=NULL;
}


int Properties::getCurrentSize(){
	return _currentSize;
}

void Properties::print(){
#ifdef DEBUG
	DEBUG_PRINTLN("Printing properties file");
	KeyValuePair *temp=_head;
	while(temp!=NULL){
		DEBUG_PRINTLN(temp->key+" = "+temp->value+"-");
		temp=temp->next;
	}
#endif
}
