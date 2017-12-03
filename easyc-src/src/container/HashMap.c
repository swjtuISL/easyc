#include <stdlib.h>
#include <string.h>

#include "sysser.h"
#include "Object.h"
#include "HashMap.h"

static const INITIAL_BUCKETS_LENGTH = 16;
static const DEFAULT_MAX_AVERAGE_DEEP = 3;

static void * get(HashMap * const self, void *key);
static int getInt(HashMap * const self, void *key);
static float getFloat(HashMap * const self, void *key);
static void putInt(HashMap * const self, void *key, int value);
static void putFloat(HashMap * const self, void *key, double value);
static void putChars(HashMap * const self, void *key, char * value);
static void put(HashMap * const self, void *key, void * value);
static int size(HashMap * const self);
static Vector *keys(HashMap * const self);
static Vector *entries(HashMap * const self);
static void resize(HashMap * const self);
static String * toString(HashMap * const self);				// 将数据转换为字符串，方便调试。
static unsigned long defaultHash(void *obj);		// key的默认hash处理函数
static void putObject(HashMap * const self, void *key, Object * obj);


/*
* @Desc   : HashMap构造器.构造一个空的hashMap.
* @Param  : *hashFunction, hash函数, 作用于key上进行.为NULL时认为key为字符串, 并采用easyc默认的hash函数.
* @Return : 返回新的构建好的HashMap
* @Authro : Shuaiji Lu
* @Date   : 2017/11/26
*/
HashMap *newHashMap(
	unsigned long (* hashFunction)(void *obj)		// 配置hash函数
	){
	HashMap *map = (HashMap *)malloc(sizeof(HashMap));

	map->_buckets = (KVNode **)malloc(sizeof(KVNode *)*INITIAL_BUCKETS_LENGTH);
	for (int i = 0; i < INITIAL_BUCKETS_LENGTH; i++){
		map->_buckets[i] = NULL;
	}
	map->_size = 0;
	map->_bucketsLength = INITIAL_BUCKETS_LENGTH;
	map->_maxAverageDeep = DEFAULT_MAX_AVERAGE_DEEP;

	map->_relative = newVector();

	// load function
	map->get = get;
	map->getInt = getInt;
	map->getFloat = getFloat;
	map->put = put;
	map->putInt = putInt;
	map->putFloat = putFloat;
	map->putChars = putChars;
	map->size = size;
	map->keys = keys;
	map->entries = entries;
	map->toString = toString;
	map->_resize = resize;

	map->_hash = hashFunction == NULL ? defaultHash : hashFunction;

	return map;
}

/*
* @Desc   : 释放hashMap.仅仅释放容器, 不会释放掉容器中所引用的内存.
* @Param  : *map, 待释放的map.
* @Return : 无
* @Authro : Shuaiji Lu
* @Date   : 2017/11/26
*/
void freeHashMap(HashMap * const map){
	freeVector(map->_relative);
	for (int i = 0; i < map->_bucketsLength; i++){
		KVNode *node = map->_buckets[i];
		while (node!=NULL && node->next != NULL){			// 释放桶中链表
			KVNode *nextnode = node->next;
			free(node);
			node = nextnode;
		}
		free(node);
		map->_buckets[i] = NULL;
	}
	free(map->_buckets);
	map->_buckets = NULL;
	free(map);
}

/*
* @Desc   : 获取hashmap中key所对应的val.
* @Param  : *self, 待操作的hashMap.
* @Param  : *key, 获取该key对应的val.
* @Return : 返回key对应的val.
* @Authro : Shuaiji Lu
* @Date   : 2017/11/26
*/
static void * get(HashMap * const self, void *key){
	int idx = self->_hash(key) % self->_bucketsLength;

	for (KVNode *node = self->_buckets[idx]; node != NULL; node = node->next){
		if (strcmp(node->key, key)==0){
			return node->obj->item;
		}
	}
	return NULL;
}

static int getInt(HashMap * const self, void *key){
	int idx = self->_hash(key) % self->_bucketsLength;

	for (KVNode *node = self->_buckets[idx]; node != NULL; node = node->next){
		if (strcmp(node->key, key) == 0){
			return (int)node->obj->item;
		}
	}
	return NULL;
}
static float getFloat(HashMap * const self, void *key){
	int idx = self->_hash(key) % self->_bucketsLength;

	for (KVNode *node = self->_buckets[idx]; node != NULL; node = node->next){
		if (strcmp(node->key, key) == 0){
			return *(double *)node->obj->item;
		}
	}
	reportError("无法获取Float数据", 1);
}


static void putObject(HashMap * const self, void *key, Object * obj){
	int idx = 0;
	KVNode *bucketop = NULL;

	self->_resize(self);			// 是否进行扩充

	idx = self->_hash(key) % self->_bucketsLength;
	bucketop = self->_buckets[idx];

	for (KVNode *node = self->_buckets[idx]; node != NULL; node = node->next){
		if (strcmp(node->key, key) == 0){	// 覆盖
			void *old = node->obj;
			node->obj = obj;
			freeObject(old);
			return;
		}
	}

	KVNode * newnode = (KVNode *)malloc(sizeof(KVNode));
	newnode->key = key;
	newnode->obj = obj;
	newnode->next = bucketop;
	self->_buckets[idx] = newnode;
	self->_size += 1;
}

/*
* @Desc   : 设置hashmap中key所对应的val.若不存在该key则添加key, 若key已经存在则覆盖key所对应的val.
* @Param  : *self, 待操作的hashMap.
* @Param  : *key, 获取该key对应的val.
* @Param  : *value, 设置key的值为value.
* @Return : 返回key之前所对应的val.
* @Authro : Shuaiji Lu
* @Date   : 2017/11/26
*/
static void put(HashMap * const self, void *key, void * value, void(*freeMethod)(void *),String *(itemToString)(void *),void*(*itemCopy)(void *)){
	putObject(self, key, newObject(value, freeMethod, itemToString, itemCopy));
}

static void putInt(HashMap * const self, void *key, int value){
	putObject(self, key, newIntObject(value));
}
static void putFloat(HashMap * const self, void *key, double value){
	putObject(self, key, newFloatObject(value));
}
static void putChars(HashMap * const self, void *key, char * value){
	putObject(self, key, newCharsObject(value));
}

/*
* @Desc   : 返回HashMap中的kv对个数.
* @Param  : *self, 待操作的hashMap.
* @Return : 无
* @Authro : Shuaiji Lu
* @Date   : 2017/11/26
*/
static int size(HashMap * const self){
	return self->_size;
}

/*
* @Desc   : 以Vector的形式返回HashMap中所有的key.
* @Param  : *self, 待操作的hashMap.
* @Return : 无
* @Authro : Shuaiji Lu
* @Date   : 2017/11/26
*/
static Vector *keys(HashMap * const self){
	Vector *vector = newVector();
	for (int i = 0; i < self->_bucketsLength; i++){
		for (KVNode *node = self->_buckets[i]; node != NULL; node = node->next){
			vector->add(vector, node->key);
		}
	}
	self->_relative->addObject(self->_relative, vector, freeVector, NULL, NULL);
	return vector;
}

/*
* @Desc   : 以Vector的形式返回HashMap中所有的kv对.
* @Param  : *self, 待操作的hashMap.
* @Return : 无
* @Authro : Shuaiji Lu
* @Date   : 2017/11/26
*/
static Vector *entries(HashMap * const self){
	Vector *vector = newVector();
	for (int i = 0; i < self->_bucketsLength; i++){
		for (KVNode *node = self->_buckets[i]; node != NULL; node = node->next){
			Entry *entry = (Entry *)malloc(sizeof(Entry));
			entry->key = node->key;
			entry->value = node->obj;
			vector->add(vector, entry);
		}
	}
	self->_relative->addObject(self->_relative, vector, freeVector, NULL, NULL);
	return vector;
}

/*
* @Desc   : 对桶进行扩容操作.当添加一个kv对时, 桶的平均深度大于maxAverageDeep时, 将会对桶进行扩容操作.
* @Param  : *self, 待操作的hashMap.
* @Return : 无
* @Authro : Shuaiji Lu
* @Date   : 2017/11/26
*/
static void resize(HashMap * const self){
	float averageDeep = (float)(self->_size + 1) / self->_bucketsLength;
	if (averageDeep < self->_maxAverageDeep){
		return;
	}
	// 新的桶和桶大小
	int newBucketsLength = self->_bucketsLength * 2;
	KVNode **newBuckets = (KVNode **)malloc(sizeof(KVNode *)*newBucketsLength);
	for (int i = 0; i < newBucketsLength; i++){
		newBuckets = NULL;
	}
	// 旧桶节点移动到新桶
	for (int i = 0; i < self->_bucketsLength; i++){
		KVNode *tmpnode = NULL;
		for (KVNode *node = self->_buckets[i]; node != NULL; node = tmpnode->next){
			int idx = self->_hash(node->key) % newBucketsLength;
			KVNode * bucketop = newBuckets[idx];
			tmpnode = node->next;
			node->next = bucketop;
			newBuckets[idx] = node;
		}
	}
	free(self->_buckets);
	self->_buckets = newBuckets;
	self->_bucketsLength = newBucketsLength;
}

String * toString(HashMap * const self){
	String *s = newString("");

	for (int i = 0; i < self->_bucketsLength; i++){
		for (KVNode *node = self->_buckets[i]; node != NULL; node = node->next){
			s->append(s, "[")->append(s, node->key)->append(s, "=")
				->appendString(s, node->obj->toString(node->obj))->append(s, "],");
		}
	}
	self->_relative->addObject(self->_relative, s, freeString, NULL, NULL);
	return s;
}

/*
* @Desc   : 对字符串的默认hash处理.
* @Param  : *obj, 应为字符串首地址.
* @Return : 返回对应的hash值.
* @Authro : Shuaiji Lu
* @Date   : 2017/11/26
*/
static unsigned long defaultHash(void *obj){
	char *s = (char *)obj;
	unsigned long val = 0;
	for (int i = 0; s[i] != '\0'; i++){
		val += s[i];
	}
	return val;
}