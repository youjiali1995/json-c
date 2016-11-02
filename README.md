# JSON-C

自学《编译器设计》前端后练手写的一个递归下降的JSON解析器和生成器，符合JSON标准。使用C89编写，手动进行 Unicode 和 UTF-8 编码和解码。

---

## 基本用法

**注意事项**:

1. 需要自己分配`json_value`传递给相应函数进行解析或生成。
2. `json_value`在使用之前需要调用`json_init`进行初始化。
3. 在使用完毕后，需要调用`json_free`释放以防内存泄露。

举例：
```C
/* Parse */
#include <stdio.h>
#include "json.h"

int main(void)
{
    json_value v;
    
    json_init(&v);
    json_parse(&v, "{\"s\": \"json\"}");
    /* %s == "json" */
    printf("%s\n", json_get_string(json_get_object_value(&v, "s")));
    json_free(&v);
    return 0;
}

/* Jsonify */
#include <stdio.h>
#include "json.h"

int main(void)
{
    char *json;
    json_value v, s;
    
    json_init(&s);
    json_set_string(&s, "json", 4);
    json_init(&v);
    json_object_append(&v, 1, "s", (size_t) 1, &s, NULL);
    json = json_jsonify(&v, NULL);
    /* json == "{\"s\": \"json\"}" */
    printf("%s\n", json);
    json_free(&s);
    json_free(&v);
    free(json);
    return 0;
}
```

----------------------------------

## **DOC**

### 数据类型

`json_type`  

JSON数据类型：JSON\_NULL, JSON\_TRUE, JSON\_FALSE, JSON\_STRING, JSON\_NUMBER, JSON\_ARRAY, JSON\_OBJECT。

`json_value`  

用于解析和生成JSON的数据类型(结构体), 内部存储JSON类型和对应数据：

 * 类型字段;
 * 值
    * JSON\_STRING: 字符串和长度
    * JSON\_NUMBER: 双精度浮点数
    * JSON\_ARRAY: 数组和数组大小
    * JSON\_OBJECT: 键值对和键值对对数

### 常量

`JSON_PARSE_OK`  

由 json\_parse 返回表明解析JSON成功。  


`JSON_PARSE_ERROR`  

由 json\_parse 返回表明解析JSON失败。  


### API

`void json_init(json_value *v);`  

初始化 v，需要在使用其余函数之前调用。  


`void json_free(json_value *v);`  

释放 v 中数据，不会释放 v ，并将 v 类型设置为 JSON\_NULL，避免重复释放。在使用完 v 之后调用，防止内存泄露(**在释放 JSON\_ARRAY 和 JSON\_OBJECT 时需注意**)。  


`int json_parse(json_value *v, const char *json);`  

JSON解析函数，成功返回 JSON\_PARSE\_OK ，并设置 v ，字符串支持 Unicode 并以 UTF-8 编码方式存储； 失败返回 JSON\_PARSE\_ERROR , 表明 json 不合法。  


`char *json_jsonify(const json_value *v, size_t *len);`  

JSON生成函数，成功返回JSON字符串，如果 len != NULL, len 被设置为JSON长度(长度均不包含结尾'\0')，使用完需释放JSON以防内存泄露。
失败返回NULL， 表明 v 不合法。  


`int json_get_type(const json_value *v);`  

返回 v 的类型，也用于 JSON\_NULL, JSON\_TRUE, JSON_FALSE 的取值。  


`double json_get_number(const json_value *v);`  

对 v 类型检查，返回 JSON\_NUMBER 类型对应的值。  


`char *json_get_string(const json_value *v);`  

对 v 类型检查，返回 JSON\_STRING 类型对应字符串。  


`size_t json_get_string_length(const json_value *v);`  

对 v 类型检查，返回 JSON\_STRING 类型对应字符串长度，因为在字符串内部可能会出现'\0'，所以需要手动记录长度。  


`json_value *json_get_array_element(const json_value *v, size_t index);`  

对 v 类型检查，返回 JSON\_ARRAY 类型给定索引对应的元素，索引从 0 开始，对 index 进行有效性检查。  


`size_t json_get_array_size(cosnt json_value *v);`  

对 v 类型检查，返回 JSON\_ARRAY 类型数组大小。  


`size_t json_get_object_size(const json_value *v);`  

对 v 类型检查，返回 JSON\_OBJECT 类型键值对数目。  


`char *json_get_object_key(const json_value *v, size_t index);`  

对 v 类型检查， 返回 JSON\_OBJECT 类型给定索引对应的键，索引从 0 开始，对 index 进行有效性检查。  


`size_t json_get_object_key_length(const json_value *v, size_t index);`  

同上， 返回对应键长。  


`json_value *json_get_object_value_index(const json_value *v, size_t index);`  

同上， 返回对应值。  


`json_value *json_get_object_value_n(const json_value *v, char *key, size_t len);`  

对 v 类型检查， 返回 JSON\_OBJECT 类型给定键和键长对应的值，失败返回 NULL 。  


`json_value *json_get_object_value(const json_value *v, char *key);`  

同上，对上面函数的包装，只能传递字面值字符串key或无空闲数组，不能传递指针。  


`void json_set_null(json_value *v);`  

`void json_set_true(json_value *v);`  

`void json_set_false(json_value *v);`  

设置 v 为 对应类型，如果传入之前使用过的 v , 需要先自行释放(set函数都需要这么做)。  


`void json_set_string(json_value *v, const char *string, size_t len);`  

设置 v 为 JSON\_STRING，并在 v 中分配内存拷贝一份 string ，并设置长度。  


`void json_set_number(json_value *v, double number);`  

设置 v 为 JSON\_NUMBER, 并设置数值。  


`void json_set_array(json_value *v, int deepcopy, ...);`  

设置 v 为 JSON\_ARRAY, 并根据可变参数设置数组元素，可变参数为一系列 json\_value \*类型，以NULL结尾。 deepcopy 非0为深拷贝，0为浅拷贝。  

该函数只能设置数组元素，不能进行修改。修改数组元素可以使用 json\_get\_array\_element 得到对应元素进行修改，但不能增加或删除数组元素。  


`void json_object_append(json_value *v, int deepcopy, ...);`  

类似上面函数，设置 v 为 JSON\_OBJECT，可变参数为3个为一组设置键值对: char \*key, size\_t key\_len, json\_value \*value，以NULL结尾。key 存储类似 json\_set\_string , 会分配空间来保存，要注意 key\_len 为 size\_t 类型，传入整型常数时要进行类型转换 (size_t) ，否则会出错。 deepcopy 非0为深拷贝，0为浅拷贝。  

当传入的 v 是 JSON\_OBJECT 类型时，会在尾部添加键值对；否则，设置 v 为 JSON\_OBJECT，从头开始添加。想要修改键值对时，可以使用 json\_get\_object\_key, json\_get\_object\_value 得到对应的键或值进行修改。  


### deepcopy

在 `json_set_array` 和 `json_object_append` 中有 `deepcopy` 标志，当非0时为深拷贝，0时为浅拷贝。  

浅拷贝将传入的`json_value *`类型的值复制到`v`中，当修改或释放`json_value *`时，如果`json_value *`内部的指针指向的内存发生变化，也会反应到`v`中，比如修改`string`, `array`或`object`。当时用浅拷贝时，只需`json_free(v)`，不需要释放传入的`json_value *`。  

深拷贝将创建`json_value *`的一个副本，然后保存在`v`中，和传入的变量独立，互不影响，按需对`json_value *`进行释放。

-------------------------

## 测试
一些用法示例可以在`test/json_test.c`中看到，也可以`make`创建`json_test`进行测试。
