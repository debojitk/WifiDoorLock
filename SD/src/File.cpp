/*

 SD - a slightly more friendly wrapper for sdfatlib

 This library aims to expose a subset of SD card functionality
 in the form of a higher level "wrapper" object.

 License: GNU General Public License V3
          (Because sdfatlib is licensed with this.)

 (C) Copyright 2010 SparkFun Electronics

 */

#include <SD.h>

/* for debugging file open/close leaks
   uint8_t nfilecount=0;
*/

SDFileWrapper::SDFileWrapper(SdFile f, const char *n) {
  // oh man you are kidding me, new() doesnt exist? Ok we do it by hand!
  _file = (SdFile *)malloc(sizeof(SdFile)); 
  if (_file) {
    memcpy(_file, &f, sizeof(SdFile));
    
    strncpy(_name, n, 12);
    _name[12] = 0;
    
    /* for debugging file open/close leaks
       nfilecount++;
       Serial.print("Created \"");
       Serial.print(n);
       Serial.print("\": ");
       Serial.println(nfilecount, DEC);
    */
  }
}

SDFileWrapper::SDFileWrapper(void) {
  _file = 0;
  _name[0] = 0;
  //Serial.print("Created empty file object");
}

// returns a pointer to the file name
char *SDFileWrapper::name(void) {
  return _name;
}

// a directory is a special type of file
boolean SDFileWrapper::isDirectory(void) {
  return (_file && _file->isDir());
}


size_t SDFileWrapper::write(uint8_t val) {
  return write(&val, 1);
}

size_t SDFileWrapper::write(const uint8_t *buf, size_t size) {
  size_t t;
  if (!_file) {
    setWriteError();
    return 0;
  }
  _file->clearWriteError();
  t = _file->write(buf, size);
  if (_file->getWriteError()) {
    setWriteError();
    return 0;
  }
  return t;
}

int SDFileWrapper::peek() {
  if (! _file) 
    return 0;

  int c = _file->read();
  if (c != -1) _file->seekCur(-1);
  return c;
}

int SDFileWrapper::read() {
  if (_file) 
    return _file->read();
  return -1;
}

// buffered read for more efficient, high speed reading
int SDFileWrapper::read(void *buf, uint16_t nbyte) {
  if (_file) 
    return _file->read(buf, nbyte);
  return 0;
}

int SDFileWrapper::available() {
  if (! _file) return 0;

  uint32_t n = size() - position();

  return n > 0X7FFF ? 0X7FFF : n;
}

void SDFileWrapper::flush() {
  if (_file)
    _file->sync();
}

boolean SDFileWrapper::seek(uint32_t pos) {
  if (! _file) return false;

  return _file->seekSet(pos);
}

uint32_t SDFileWrapper::position() {
  if (! _file) return -1;
  return _file->curPosition();
}

uint32_t SDFileWrapper::size() {
  if (! _file) return 0;
  return _file->fileSize();
}

char * SDFileWrapper::getStringDate(char * dateStr){
	if(! _file){
		return NULL;
	}else{
		char datePart[5];
		strcpy(dateStr,itoa(FAT_YEAR(_file->createDate()), datePart, 10));
		strcat(dateStr,"-");
		strcat(dateStr,_file->printTwoDigits(FAT_MONTH(_file->createDate()), datePart));
		strcat(dateStr,"-");
		strcat(dateStr,_file->printTwoDigits(FAT_DAY(_file->createDate()), datePart));
		return dateStr;
	}
}
char * SDFileWrapper::getStringTime(char * dateStr){
	if(! _file){
		return NULL;
	}else{
		char datePart[3];
		strcpy(dateStr,_file->printTwoDigits(FAT_HOUR(_file->createTime()), datePart));
		strcat(dateStr,"-");
		strcat(dateStr,_file->printTwoDigits(FAT_MINUTE(_file->createTime()), datePart));
		strcat(dateStr,"-");
		strcat(dateStr,_file->printTwoDigits(FAT_SECOND(_file->createTime()), datePart));
		return dateStr;
	}
}
uint16_t SDFileWrapper::getDate(){
	  if (! _file) return 0;
	  return _file->createDate();
}
uint16_t SDFileWrapper::getTime(){
	  if (! _file) return 0;
	  return _file->createTime();
}

void SDFileWrapper::close() {
  if (_file) {
    _file->close();
    free(_file); 
    _file = 0;

    /* for debugging file open/close leaks
    nfilecount--;
    Serial.print("Deleted ");
    Serial.println(nfilecount, DEC);
    */
  }
}

SDFileWrapper::operator bool() {
  if (_file) 
    return  _file->isOpen();
  return false;
}

