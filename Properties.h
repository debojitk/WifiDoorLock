/*
 * Properties.h
 *
 *  Created on: Sep 11, 2016
 *      Author: debojitk
 */

#ifndef PROPERTIES_H_
#define PROPERTIES_H_
#include "Properties.h"
#include <stddef.h>
#include <Arduino.h>
#include <FS.h>
#define MAX_SIZE 10


struct KeyValuePair{
	String key;
	String value;
	KeyValuePair *next;
};


class Properties {

public:
	Properties(int size);
	virtual ~Properties();
	void load(String fileName);
	void store();
	void store(String fileName);
	String get(String key);
	void put(String key, String value);
	String remove(String key);
	void removeAll();
	void parsePropertiesAndPut(String inputStr);
	int getCurrentSize();
	void print();
	String serialize();
private:
	uint8_t _size;
	uint8_t _currentSize=0;
	KeyValuePair *_head;
	KeyValuePair *_tail;

};

#endif /* PROPERTIES_H_ */
