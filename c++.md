#### 拷贝构造

```cpp
#include <iostream>
#include <unistd.h>
using namespace std;

class MyClass
{
private:
    int age;
public:
    int ID;
    double scores;
    string name;
    int *widget;
    MyClass(int a, string b, int c, double d, int w)
        :ID(a),name(b),age(c),scores(d),widget(new int(w))
    {
        cout << "构造" << endl;
    }
    MyClass(const MyClass& obj)
        :ID(obj.ID),name(obj.name),age(obj.age),scores(obj.scores),widget(new int(*obj.widget))
    {
        cout << "拷贝" << endl;
    }
    ~MyClass()
    {
        delete widget;
        widget = NULL;
        cout << "析构" << endl;
    }
    void show_varibale()
    {
        cout << "ID:" << ID << endl;
        cout << "name:" << name << endl;
        cout << "age:" << age << endl;
        cout << "score:" << scores << endl;
        cout << "widget:" << widget << endl;
    }
};

int main()
{
    MyClass mc1(1111, "小明", 18, 99, 70);
    mc1.show_varibale();

    MyClass *mc2 = new MyClass(2222, "小李", 22, 96, 78);
    mc2->show_varibale();


    MyClass mc3 = mc1;
    mc3.show_varibale();

    MyClass mc4(mc1);
    mc4.show_varibale();

    delete mc2;
    mc2 = NULL;

    return 0;
}

```

##### 输出结果

```
构造
ID:1111
name:小明
age:18
score:99
widget:0x1d17a0
构造
ID:2222
name:小李
age:22
score:96
widget:0x1d17c0
拷贝
ID:1111
name:小明
age:18
score:99
widget:0x1d1b10
拷贝
ID:1111
name:小明
age:18
score:99
widget:0x1d1fa0
析构
析构
析构
析构

```

#### 委托构造函数

```cpp
#include <iostream>
#include <unistd.h>
using namespace std;

class Person
{
public:
    Person():Person("魏士雄","帅"){cout << "委托人" << endl;}
    Person(string name, string feature)
        :name(name),feature(feature)
    {
        cout << "姓名：" << name << endl;
        cout << "特征：" << feature << endl;
    }
private:
    string name;
    string feature;
};

int main()
{
    Person p1;
    Person p2("Tom","handsome");

    return 0;
}
```

eg:

```cpp
#include <iostream>
#include <unistd.h>
using namespace std;

class MyClassA
{
public:
    MyClassA(int max):m_max(max)
    {
        cout << m_max << endl;
    }
    MyClassA(int max, int min):MyClassA(max)
    {
        m_min = max + min + m_max;
        cout << m_max << " " << m_min << endl;
    }
    MyClassA(int max, int min, int middle):MyClassA(max, min)
    {
        m_middle = m_max + m_min + middle;
        cout << m_max << " " << m_min << " " << m_middle << endl;
    }
private:
    int m_max;
    int m_min;
    int m_middle;
};

int main()
{
    MyClassA my(1,1,1);

    return 0;
}

```

#### 移动构造

```cpp
#include <iostream>
#include <unistd.h>
using namespace std;

class Person
{
public:
    string name;
    string feature;
    Person(const string &name, const string feature)
        :name(name),feature(feature)
    {
        cout << "姓名：" << name << endl;
        cout << "特征：" << feature << endl;
    }
    Person(Person && p):name(move(p.name)),feature(move(p.feature)){

    }
};

int main()
{
    Person lisi("李四","爱上学");
    Person wangwu(move(lisi));
    cout << wangwu.name << endl;
    cout << wangwu.feature << endl;
    cout << lisi.name << endl;
    cout << lisi.feature << endl;

    return 0;
}



```

#### 常函数

```cpp
#include <iostream>
#include <unistd.h>
using namespace std;

class Person
{
public:
    Person(int id, int pass):ID(id),pass(pass)
    {
        cout << "Person(int id, int pass)" << endl;
    }
    ~Person()
    {
        cout << "~Person()" << endl;
    }

    void print()
    {
        cout << "普通函数版本" << endl;
        ID = 100;
        pass = 200;
        cout << "ID:" << ID << endl;
        cout << "pass:" << pass << endl;
    }

    //常函数版本
    //普通函数版本可以和常函数版本构成重载
    //当普通函数版本和常函数版本都存在的情况下，会优先调用普通函数版本
    //如果没有普通函数版本，则会调用常函数版本
    void print()const
    {
        cout << "常函数版本" << endl;
        ID = 123;
        cout << "ID:" << ID << endl;
        cout << "pass:" << pass << endl;
    }

private:
    mutable int ID;//加上mutable关键字后，常函数版本可以对成员变量进行修改
    int pass;
};

int main()
{
    Person dh(200,300);
    dh.print();

    return 0;
}


```

#### 静态成员变量

```cpp
#include <iostream>
#include <unistd.h>
using namespace std;

class Person
{
public:
    int pub_a;
    static int pub_s;
    void print()
    {
        cout << "pub_s:" << pub_s << endl;
        cout << "pri_s:" << pri_s << endl;
        cout << "pro_s:" << pro_s << endl;
     }
private:
    int pri_b;
    static int pri_s;
protected:
    int pro_c;
    static int pro_s;
};

int Person::pub_s = 1;
int Person::pri_s = 2;
int Person::pro_s = 3;

int main()
{
    cout << Person::pub_s << endl;
//    cout << Person::pri_s << endl;
//    cout << Person::pro_s << endl;
    Person dh;
    dh.print();

    Person *dh1 = new Person;
    dh1->print();
    delete dh1;
    dh1 = NULL;

    return 0;
}


```

#### 友元函数

```cpp
#include <iostream>

using namespace std;

class Person
{
public:
    friend class Person2;
    friend void print();
    friend void print(Person &s);
    string name;
    static int pub_s;
private:
    int a;
    static int pri_s;
protected:
    int age;
    static int pro_s;
};

class Person2{
public:
//    friend class Person;
    void print()
    {
        cout << Person::pub_s << endl;
        cout << Person::pri_s << endl;
        cout << Person::pro_s << endl;
    }

    void print(Person &s)
    {
        s.name = "wsx";
        s.a = 200;
        s.age = 23;
        cout << s.name << endl;
        cout << s.a << endl;
        cout << s.age << endl;
    }
};

int Person::pub_s = 100;
int Person::pri_s = 200;
int Person::pro_s = 300;

void print()
{
    cout << Person::pub_s << endl;
    cout << Person::pri_s << endl;
    cout << Person::pro_s << endl;
}

void print(Person &s)
{
    s.name = "wsx";
    s.a = 200;
    s.age = 23;
    cout << s.name << endl;
    cout << s.a << endl;
    cout << s.age << endl;
}

int main()
{
    print();
    Person p;
    print(p);
    Person2 p2;
    p2.print(p);

    return 0;
}

```

#### 赋值操作

```cpp
#include <iostream>

using namespace std;

class MyClass
{
public:
    MyClass(int data, int size):m_data(data),m_size(new int(size)){
        cout << "MyClass(int data)" << endl;
        print();
    }
    ~MyClass(){
        delete m_size;
        m_size = NULL;
        cout << "~MyClass()" << endl;
    }
    MyClass(const MyClass& obj):m_data(obj.m_data),m_size(new int(*obj.m_size)){
        cout << "Myclass(const MyClass& obj)" << endl;
        print();
    }
    MyClass &operator=(const MyClass &other){
        cout << "MyClass &operator=(const MyClass &other)" << endl;
        if (this != &other){
            cout << "赋值操作" << endl;
            m_data = other.m_data;
            int *ptr = new int(*other.m_size);
            delete m_size;
            m_size = ptr;
        }
        return *this;
    }
    void print(){
        cout << "m_data:" << m_data << endl;
        cout << "*m_size:" << *m_size << endl;
    }
private:
    int m_data;
    int *m_size;
};

int main()
{
    MyClass m1(100, 1);
    MyClass m2(m1);
    MyClass m3(200, 2);
    m3 = m2;
    m3.print();

    return 0;
}
```

#### operator运算符重载

```cpp
#include <iostream>

using namespace std;

class Complex{
public:
    friend const Complex operator+ (const Complex& L,const Complex& R);
    friend bool operator< (const Complex& L, const Complex& R);
    friend const Complex operator- (const Complex& O);
    friend bool operator! (const Complex& O);
    friend Complex& operator-= (Complex& L, Complex& R);
    Complex():m_a(0),m_b(0){
        cout << "这是无参构造" << endl;
    }
    Complex(int m_a, int m_b):m_a(m_a),m_b(m_b){
        cout << "这是有参构造" << endl;
    }
    ~Complex(){
        cout << "这是析构函数" << endl;
    }
    void print(){
        cout << m_a << "+" << m_b << "i" << endl;
    }
//    const Complex operator+ (const Complex& R)const{
//        Complex temp;
//        temp.m_a = this->m_a + R.m_a;
//        temp.m_b = this->m_b + R.m_b;
//        return temp;
//    }
//    bool operator< (const Complex& R)const{
//        if (this->m_a < R.m_a && this->m_b < R.m_b){
//            return true;
//        }else{
//            return false;
//        }
//    }
private:
    bool flag = false;
    int m_a;
    int m_b;
};

const Complex operator+ (const Complex& L,const Complex& R){
    Complex temp;
    temp.m_a = L.m_a + R.m_a;
    temp.m_b = L.m_b + R.m_b;
    return temp;
}

bool operator< (const Complex& L, const Complex& R){
    if (L.m_a < R.m_a && L.m_b < R.m_b){
        return true;
    }else{
        return false;
    }
}

const Complex operator- (const Complex& O){
    Complex temp;
    temp.m_a = -O.m_a;
    temp.m_b = -O.m_b;
    return temp;
}

bool operator! (const Complex& O){
    if (O.flag == true){
        return false;
    }else{
        return true;
    }
}

Complex& operator-= (Complex& L, Complex& R)
{
    L.m_a -= R.m_a;
    L.m_b -= R.m_b;
    return L;
}

int main()
{
    Complex c1(1,1);
    Complex c2(2,2);
    Complex c3 = c1 + c2;
    c3.print();
    if (c1 < c2){
        cout << "c1 < c2" << endl;
    }
    else{
        cout << "c1 > c2" << endl;
    }
    Complex c4 = -c2;
    c4.print();
    c4 -= c2;
    c4.print();
    cout << boolalpha << !c1 << endl;

    return 0;
}

```

#### 继承

```cpp
#include <iostream>

using namespace std;

class Base{
public:
    int pub_base;
private:
    int pri_base;
protected:
    int pro_base;
};

class Son:public Base{
public:
    int pub_son;
    void base_func(){
        cout << "pub_base:" << pub_base << endl;
        cout << "pro_base:" << pro_base << endl;
//        cout << "pri_base:" << pri_base << endl;
    }
private:
    int pri_son;
};

int main()
{
    Son s1;
    s1.pub_base = 10;
    s1.pub_son = 20;
    cout << s1.pub_base << endl;
    cout << s1.pub_son << endl;
    s1.base_func();

    return 0;
}


```

eg:

```cpp
#include <iostream>

using namespace std;

class Base{
public:
    bool sex;
    string name;
    void base_func()
    {
        sex = true;
        cout << "name:" << name << endl;
        cout << "sex:" << sex << endl;
        cout << "age:" << age << endl;
    }
protected:
    int age;
};

class Son:public Base{
public:
    bool sex;
    void son_print()
    {
        sex = false;
        age = 18;
        base_func();
        cout << sex << endl;
        cout << Base::sex << endl;
        cout << score << endl;
    }
protected:
    int score;
};

int main()
{
    Son s1;
    s1.name = "wsx";
    s1.son_print();
    cout << boolalpha << s1.sex << endl;
    cout << s1.Base::sex << endl;

    return 0;
}



```

eg;

```cpp
#include <iostream>

using namespace std;

class Base{
public:
    Base(int data):base_data(data){
        cout << "Base(int data)" << endl;
    }
    Base(const Base& obj):base_data(obj.base_data){
        cout << "Base(const Base& obj" << endl;
    }
    Base& operator= (const Base& obj){
        cout << "Base &operator= (const Base& obj)" << endl;
        if (this != &obj){
            base_data = obj.base_data;
        }
        return *this;
    }
    ~Base(){
        cout << "~Base" << endl;
    }
    void base_print(){
        cout << "base_data:" << base_data << endl;
    }
protected:
    int base_data;
};

class Son: public Base{
public:
    Son(int a, int b):Base(a),son_data(b){
        cout << "Son(int a, int b)" << endl;
    }
    Son(const Son& s):Base(s),son_data(s.son_data){
        cout << "Son(const Son& s)" << endl;
    }
    Son& operator= (const Son& s){
        cout << "Son &operator= (const Son& s)" << endl;
        if (this != &s){
            Base::operator=(s);
            son_data = s.son_data;
        }
        return *this;
    }
    ~Son(){
        cout << "~Son" << endl;
    }
    void son_print(){
        base_print();
        cout << "son_data:" << son_data << endl;
    }
protected:
    int son_data;
};

int main()
{
    Son s1(10,20);
    s1.son_print();

    Son *s2 = new Son(100,200);
    s2->son_print();
    delete s2;
    s2 = NULL;

    Son s3(30,40);
    s3.son_print();
    s3 = s1;
    s3.son_print();


    return 0;
}

```

eg:

```cpp
#include <iostream>

using namespace std;

class Base{
public:
    int value;
    int a;
};

class Base1{
public:
    int value;
    int b;
};

class Son: public Base, public Base1{
public:
    void print(){
        Base::value = 100;
        Base1::value = 200;
        Base::a = 1;
        Base1::b = 2;
//        cout << value << endl;
        cout << "Base::value " << Base::value << endl;
        cout << "Base1::value " << Base1::value << endl;
        cout << "a " << a << endl;
        cout << "b " << b << endl;
    }
};

int main()
{
    Son s;
    s.print();

    return 0;
}

```

eg:

```cpp
#include <iostream>

using namespace std;

class A{
protected:
    int pro_A;
};

class B: public A{
protected:
    int pro_B;
};

class C: public B{
public:
    void print(){
        pro_A = 1;
        pro_B = 2;
        pro_C = 3;
        cout << pro_A << endl;
        cout << pro_B << endl;
        cout << pro_C << endl;
    }
protected:
    int pro_C;
};

int main()
{
    C c;
    c.print();

    return 0;
}

```

eg:

```cpp
#include <iostream>

using namespace std;

class Add{
public:
    int add(){
        return a + b;
    }
protected:
    int a;
    int b;
};

class Sub{
public:
    int sub(){
        return a - b;
    }
protected:
    int a;
    int b;
};

class AddSub: public Add, public Sub{
public:
    void print(){
        Add::a = 1;
        Add::b = 2;
        Sub::a = 9;
        Sub::b = 4;
        cout << "Add " << add() << endl;
        cout << "Sub " << sub() << endl;
    }
protected:
    int pro_C;
};

int main()
{
    AddSub c;
    c.print();

    return 0;
}

```

eg:

```cpp
#include <iostream>

using namespace std;

class Animal{
public:
    Animal(int a):a(a){
        cout << "Animal: " << a << endl;
    }
protected:
    int a;
};

class Horse: virtual public Animal{
public:
    Horse(int a, int h):Animal(a),h(h){
        cout << "Horse: " << h << endl;
    }
protected:
    int h;
};

class Cow: virtual public Animal{
public:
    Cow(int a, int c):Animal(a),c(c){
        cout << "Cow: " << c << endl;
    }
protected:
    int c;
};

class HorseCow: public Horse, public Cow{
public:
    HorseCow(int a, int b, int c, int hc):Animal(a),Horse(a,b),Cow(a,c),hc(hc){
        cout << "HorseCow: " << hc << endl;
    }
protected:
    int hc;
};

int main()
{
//    Animal a(1);
    HorseCow m(1, 2, 3, 4);

    return 0;
}

```

#### 多态:

```cpp
#include <iostream>

using namespace std;

class Base{
public:
    virtual void print(){
        cout << "Base::print()" << endl;
    }
    virtual ~Base() {
        cout << "~Base()" << endl;
    }
};

class Son1:public Base{
public:
    void print(){
        cout << "Son1::print()" << endl;
    }
    ~Son1(){
        cout << "~Son1()" << endl;
    }
};

class Son2:public Base{
public:
    void print(){
        cout << "Son2::print()" << endl;
    }
    ~Son2(){
        cout << "~Son2()" << endl;
    }
};

int main()
{
    //指向子类的父类指针
    Son1 s1;
    s1.print();
    Base *b1 = &s1;
    b1->print();

    Base *b2 = new Son2;
    b2->print();
    delete b2;
    b2 = NULL;

    return 0;
}

```

#### 静态与动态多态

```cpp
#include <iostream>

using namespace std;

class Animal{
public:
    virtual void print(){
        cout << "动物声音" << endl;
    }
    virtual ~Animal() {
        cout << "~Animal()" << endl;
    }
};

class Dog:public Animal{
public:
    void print(){
        cout << "汪汪" << endl;
    }
    ~Dog(){
        cout << "~Dog()" << endl;
    }
};

class Cat:public Animal{
public:
void print(){
        cout << "喵喵" << endl;
    }
    ~Cat(){
        cout << "~Cat()" << endl;
    }
};

void print(Animal &a){
    a.print();
}

void print(Dog &a){
    a.print();
}

void print(Cat &a){
    a.print();
}

int main()
{
    {
        cout << "静态：" << endl;
        Animal a;
        print(a);

        Dog d;
        d.print();

        Cat c;
        c.print();
    }
    cout << "\n动态：" << endl;

    Animal *a1 = new Dog;
    print(*a1);

    Animal *a2 = new Cat;
    print(*a2);

    delete a2;
    a2 = NULL;
    delete a1;
    a1 = NULL;

    return 0;
}

```
#### 纯虚函数和抽象类

```cpp
#include <iostream>

using namespace std;

class Animal{
public:
    Animal(string name, int age):name(name), age(age){
        cout << "Animal(string name,int age)" << endl;
    }
    virtual void print() = 0;
    virtual ~Animal() {
        cout << "~Animal()" << endl;
    }
protected:
    string name;
    int age;
};

class Cat:public Animal{
public:
    Cat(string name, int age, string color):Animal(name, age), color(color){
        cout << "Cat(string name,int age, string color)" << endl;
    }
    void print(){
        cout << "喵喵" << endl;
    }
    ~Cat(){
        cout << "~Cat()" << endl;
    }
protected:
    string color;
};


int main()
{
    Animal *a = new Cat("Tom", 19, "blue");
    a->print();
    delete a;
    a = NULL;

    return 0;
}

```

#### 函数模板

```cpp
#include <iostream>

using namespace std;


template <class T>
void MySwap(T &a, T &b)
{
    auto temp = a;
    a = b;
    b = temp;
    cout << a << " " << b << endl;
}


int main()
{
    int a = 10, b = 20;
    double c = 1.2, d = 2.4;
    char e = 'a', f = 'b';
    MySwap<int>(a, b);
    MySwap<double>(c, d);
    MySwap<char>(e, f);
//    cout << a << " " << b << endl;
//    cout << c << " " << d << endl;
//    cout << e << " " << f << endl;

    return 0;
}

```

#### 类模板

```cpp
#include <iostream>

using namespace std;
template <typename T>
class Box{
public:
    Box(T v):value(v){}
    T get()const;
    void set(T v);
private:
    T value;
};

template <typename T>
T Box<T>::get() const{
return value;
}

template <typename T>
void Box<T>::set(T v){
    value = v;
}

int main()
{
    Box<int> inbox(42);
    Box<double> dbox(3.14);
    Box<string> sbox("hello");
    inbox.set(30);
    sbox.set("world");
    cout << sbox.get() << endl;
    cout << inbox.get() << endl;
    cout << dbox.get() << endl;

    return 0;
}


eg:
​```cpp
#include <iostream>

using namespace std;


template <typename T>
class Box{
    T value;
public:
    Box(T v):value(v){}
    friend ostream& operator<<(ostream&os, const Box<T>&b)
    {
        return os << "Box(" << b.value << ")" << endl;
    }
};


int main()
{
    Box<int> b(42);
    cout << b << endl;

    return 0;
}

```

eg:

```cpp
#include <iostream>

using namespace std;


template <int N>
struct Arr{
    int buf[N];
};

enum Color{RED = 1};
template <Color C>
void print(){
    cout << "Color" << C <<endl;
}

int main()
{
    Arr<5> a{};
    print<RED>();

    return 0;
}

```

#### 容器

**C++容器总结**

C++标准库提供了多种容器，用于存储和管理数据。容器可以分为以下几类：

1. **序列容器**：按顺序存储元素
   - vector：动态数组，支持快速随机访问
- list：双向链表，支持快速插入和删除
   - deque：双端队列，支持两端的快速插入和删除
2. **关联容器**：按键排序并存储元素
   - set/multiset：集合，存储唯一/可重复的键，自动排序
   - map/multimap：映射，存储键值对，自动按键排序
3. **无序关联容器**：基于哈希表实现，不保证顺序
4. **容器适配器**：对其他容器进行包装
   - stack：栈，后进先出
   - queue：队列，先进先出
   - priority_queue：优先队列，自动保持最大元素在队首

**容器的共性操作**：

- size()：返回容器中元素个数
- empty()：检查容器是否为空
- begin()/end()：返回指向首元素/尾元素下一位置的迭代器
- insert()：插入元素
- erase()：删除元素
- clear()：清空容器

**选择合适的容器**：

- 需要快速随机访问：vector
- 需要频繁插入删除：list
- 需要在两端操作：deque
- 需要按键查找：map
- 需要去重和排序：set

##### vector

**vector容器说明**：

vector是一种动态数组容器，支持以下特点：
- 元素在内存中连续存储，支持随机访问（通过下标操作符[]或at()方法）
- 尾部插入/删除操作效率高（均摊O(1)时间复杂度）
- 中间插入/删除操作效率较低（需要移动元素，O(n)时间复杂度）
- 当存储空间不足时会自动扩容，通常会分配原容量2倍的新空间

**常用方法**：
- push_back()：在尾部添加元素
- pop_back()：删除尾部元素
- size()：获取元素个数
- empty()：判断是否为空
- clear()：清空容器
- begin()/end()：获取首尾迭代器
- []/at()：访问指定位置元素
```cpp
#include <iostream>
#include <vector>
using namespace std;

int main()
{
    vector<int> v;
    vector<int>::iterator it; // 正向迭代器
    vector<int>::reverse_iterator rit; // 反向迭代器

    v.push_back(1); // 尾部添加元素
    v.push_back(2);
    v.push_back(3);
    v.push_back(4);
    v.push_back(5);
    v.push_back(6);

    for (it = v.begin(); it != v.end();){ // 正向迭代器遍历
        cout << *it << " ";
        it++;
    }
    cout << endl;
    cout << "----------------------------" <<endl;
    for (it = v.begin(); it != v.end();){ // 边遍历边删除特定元素
        if (*it % 3 == 0){ // 删除能被3整除的元素
            v.erase(it); // erase后迭代器会自动指向下一个元素
        }else{
cout << *it << " ";
            it++;
        }
    }
    cout << endl;
    cout << "----------------------------" <<endl;
    
    vector<int> v1{1, 2, 3, 4, 5}; // C++11初始化列表
    for (int i = 0; i < 5; i++){ // 传统for循环
        cout << v1[i] << " ";
    }
    cout << endl;
    cout << "----------------------------" <<endl;
    for (int num : v1){// 范围for循环（C++11特性）
        cout << num << " ";
    }
    cout << endl;

    return 0;
}



```

##### list

list容器是C++标准库中的双向链表实现，具有以下特点：

1. 在任意位置插入和删除操作效率高（O(1)时间复杂度）
2. 不支持随机访问，只能通过迭代器顺序访问
3. 元素在内存中不连续存储，每个节点包含数据和指向前后节点的指针
4. 当插入或删除元素时，除了操作点附近的节点外，其他节点不受影响
5. 迭代器在插入操作时不会失效，但在删除操作时只会使指向被删除元素的迭代器失效

list其他常用操作补充注释：

- empty()：检查容器是否为空，返回bool值
- size()：返回容器中元素的个数
- clear()：清空容器中的所有元素
- front()：访问第一个元素
- back()：访问最后一个元素
- swap(lst2)：交换两个list容器的内容
- merge(lst2)：合并两个已排序的list容器
- reverse()：反转list容器中的元素顺序
- sort()：对list容器中的元素进行排序

```cpp
#include <iostream>
#include <list> // 包含list容器头文件
using namespace std;

int main()
{
    list<int> lst1; // 创建一个空的int类型的list容器
    lst1.assign({10, 20, 30}); // 赋值操作，用初始列表替换原有元素（当前为空，所以是初始化）
    for (auto it = lst1.begin(); it != lst1.end(); it++){ // 使用auto自动推导迭代器类型进行遍历
        auto &x = *it; // 引用方式访问元素，避免拷贝
        cout << x << " ";
    }
    cout << endl;
    cout << "-------------------------" << endl;

    //push_back尾插（在链表尾部添加元素）
    //push_front头插（在链表头部添加元素）
    lst1.push_back(100); // 在尾部插入元素100
    lst1.push_front(0); // 在头部插入元素0
    for (auto it = lst1.begin(); it != lst1.end(); it++){
        auto &x = *it;
        cout << x << " ";
    }
    cout << endl;
    cout << "-------------------------" << endl;

    for (auto &x : lst1){ // 使用C++11范围for循环遍历（简化版）
        cout << x << " ";
    }
    cout << endl;
    cout << "-------------------------" << endl;

    lst1.pop_front();//删除头部第一个元素
    lst1.pop_back();//删除最后一个元素
    for (auto &x : lst1){
        cout << x << " ";
    }
    cout << endl;
    cout << "-------------------------" << endl;

    cout << *lst1.begin() << endl; // 获取并输出第一个元素
    cout << *--lst1.end() << endl; // 获取并输出最后一个元素（注意：end()指向末尾后位置，需先减1）
    cout << "-------------------------" << endl;

    auto it = ++lst1.begin(); // 获取指向第二个元素的迭代器
    lst1.insert(it, 999); // 在指定位置插入元素999
    for (auto &x : lst1){
        cout << x << " ";
    }
    cout << endl;
    cout << "-------------------------" << endl;

    it = ++lst1.begin(); // 重新获取迭代器（重要：因为插入操作可能使之前的迭代器失效，虽然list不会，但这是好习惯）
    lst1.erase(it); // 删除指定位置的元素
    for (auto &x : lst1){
        cout << x << " ";
    }
    cout << endl;
    cout << "-------------------------" << endl;

    


    return 0;
}
```



##### deque

**deque容器说明**：

deque（双端队列）是一种支持两端快速插入和删除的容器，具有以下特点：
- 元素在内存中分段连续存储，通过中央控制结构管理多个缓冲区
- 支持两端的快速插入/删除操作（O(1)时间复杂度）
- 支持随机访问（通过下标操作符[]，O(1)时间复杂度）
- 中间插入/删除操作效率较低（O(n)时间复杂度）

**常用方法**：
- push_back()：在尾部添加元素
- push_front()：在头部添加元素
- pop_back()：删除尾部元素
- pop_front()：删除头部元素
- insert()：在指定位置插入元素
- erase()：删除指定位置元素
- operator[]：随机访问元素
```cpp
#include <iostream>
#include <deque>
using namespace std;

int main()
{
    deque<int> dq;
    dq.push_back(10); // 尾部插入
    dq.push_front(5); // 头部插入
    dq.push_back(20);
    dq.push_front(15);
    for (auto it = dq.begin(); it != dq.end(); it++){
        cout << *it << " ";
    }
    cout << endl;
    cout << "--------------------------" << endl;

    dq.pop_front(); // 删除头部元素
    dq.pop_back(); // 删除尾部元素
    for (auto it = dq.begin(); it != dq.end(); it++){
        cout << *it << " ";
    }
    cout << endl;
    cout << "--------------------------" << endl;

    dq.insert(dq.begin() + 1, 99); // 在位置1插入元素
    dq.erase(dq.begin() + 2); // 删除位置2的元素
    for (auto it = dq.begin(); it != dq.end(); it++){
        cout << *it << " ";
    }
    cout << endl;
    cout << "--------------------------" << endl;
    return 0;
}


```

结果

![76223949711](C:\Users\魏士雄\AppData\Local\Temp\1762239497112.png)

##### 集合

**set/multiset容器说明**：

set和multiset都是基于红黑树实现的有序容器，具有以下特点：
- set中的元素是唯一的，自动按键升序排序
- multiset允许存储重复元素，也自动按键升序排序
- 查找、插入、删除操作的平均时间复杂度为O(log n)
- 不支持通过迭代器修改元素值（因为会破坏排序结构）

**常用方法**：
- insert()：插入元素
- erase()：删除元素（可以按值或迭代器删除）
- find()：查找元素
- count()：统计元素出现次数
- size()：获取容器大小
- empty()：判断是否为空

```cpp
#include <iostream>
#include <set>
using namespace std;

int main()
{
    set<int> s; // 集合，元素唯一且自动排序
    multiset<int> ms; // 多重集合，允许重复元素
    s.insert(5);
    ms.insert(5);
    s.insert({3, 1, 4, 1, 5}); // 插入多个元素，set会自动去重并排序
    ms.insert({3, 1, 4, 1, 5}); // 插入多个元素，multiset允许重复

    for (int x: s){ // 遍历set，输出有序且无重复的元素
        cout << x << " ";
    }
    cout << endl;
    for (int x: ms){ // 遍历multiset，输出有序但可能有重复的元素
        cout << x << " ";
    }
    cout << endl;
    cout << "-----------------------" << endl;

    cout << s.size() << endl; // set大小（去重后）
    cout << ms.size() << endl; // multiset大小（包含重复）
    cout << "-----------------------" << endl;

    s.erase(1); // 删除值为1的元素（set中最多一个）
    for (int x: s){
        cout << x << " ";
    }
    cout << endl;
    cout << "-----------------------" << endl;
    
    ms.erase(1); // 删除值为1的所有元素（multiset中可能多个）
    for (int x: s){
        cout << x << " ";
    }
    cout << endl;
    cout << "-----------------------" << endl;
 

    return 0;
}


```

结果

![76224127810](C:\Users\魏士雄\AppData\Local\Temp\1762241278104.png)

##### map映射

**map/multimap容器说明**：

map和multimap是存储键值对的有序关联容器，具有以下特点：
- map中的键是唯一的，自动按键升序排序
- multimap允许存储重复的键，也自动按键升序排序
- 查找、插入、删除操作的平均时间复杂度为O(log n)
- 通过键可以快速访问对应的值

**常用方法**：
- insert()：插入键值对
- operator[]：访问或插入键值对（仅map可用）
- erase()：删除键值对
- find()：查找键
- count()：统计键出现次数
- size()：获取容器大小
- empty()：判断是否为空

```cpp
#include <iostream>
#include <map>
using namespace std;

int main()
{
    map<string, int> mp; // map，键唯一
    multimap<string, int> mmp; // multimap，允许重复键
    mp.insert({"apple",10}); // 插入键值对
    mp["banana"] = 5; // 使用[]运算符插入或修改值
    mmp.insert({"apple", 3}); // multimap插入
    mmp.insert({"apple", 1}); // 插入重复键

    for (map<string, int>::iterator it = mp.begin(); it != mp.end(); it++) // 遍历map
    {
        cout << it->first << "->" << it->second << endl; // first访问键，second访问值
    }

    for (multimap<string, int>::iterator it = mmp.begin(); it != mmp.end(); it++) // 遍历multimap
    {
        cout << it->first << "->" << it->second << endl;
    }
    cout << "---------------------------------" << endl;

    mp.erase("apple"); // 删除键为"apple"的元素
    mmp.erase("apple"); // 删除所有键为"apple"的元素
    for (map<string, int>::iterator it = mp.begin(); it != mp.end(); it++)
    {
        cout << it->first << "->" << it->second << endl;
    }
    for (multimap<string, int>::iterator it = mmp.begin(); it != mmp.end(); it++)
    {
        cout << it->first << "->" << it->second << endl;
    }
    return 0;
}


```

结果

![76224351120](C:\Users\魏士雄\AppData\Local\Temp\1762243511201.png)

##### 函数对象

**函数对象说明**：

函数对象（Function Object），也称为仿函数（Functor），是一种重载了函数调用运算符()的类或结构体。函数对象具有以下特点：
- 可以像函数一样被调用
- 可以拥有状态（成员变量）
- 可以作为参数传递给其他函数
- 比普通函数更灵活，可以存储信息

**函数对象类型**：
1. 一元函数：接受一个参数并返回一个值
2. 一元谓词：接受一个参数并返回布尔值
3. 二元函数：接受两个参数并返回一个值
4. 二元谓词：接受两个参数并返回布尔值
```cpp
#include <iostream>
#include <map>
using namespace std;
//一元函数
int A(int x){
    return x * x;
}
//一元谓词
bool B(int a){
    return a % 2 == 0;
}
//二元函数
int C(int a, int b){
    return a + b;
}
//二元谓词
bool D(int a, int b){
    return a > b;
}

class Adder{
public:
    int operator()(int a, int b){
        return a + b;
    }
};

int main()
{
    Adder adder;
//   int result = adder(5, 3);
    int result = adder.operator()(5, 5);
    cout << result << endl;
    return 0;
}


##### functional

**functional模板说明**：

C++11引入的`<functional>`头文件提供了`function`类模板，用于封装各种可调用对象（函数、函数指针、函数对象、lambda表达式等），具有以下特点：
- 可以存储和传递可调用对象
- 提供统一的接口来调用不同类型的可调用对象
- 可以作为回调函数使用
- 支持空状态检查（检查是否绑定了可调用对象）

**基本语法**：
​```cpp
function<返回类型(参数类型列表)> 函数对象名;
```

**常用操作**：
- 赋值操作：将可调用对象赋值给function对象
- 函数调用：通过`function对象(参数)`形式调用封装的可调用对象
- 布尔转换：检查function对象是否为空（是否绑定了可调用对象）

```cpp
#include <iostream>
#include <functional> // 必须包含此头文件
sing namespace std;

int add(int a, int b){ // 普通函数
    return a + b;
}

int main()
{
    function<int(int, int)> fun; // 声明一个function对象，返回类型int，接受两个int参数
    cout << (fun?"否":"是") << endl; // 检查function对象是否为空（未绑定任何可调用对象）
    fun = add; // 将普通函数add赋值给function对象
    cout << fun(5, 3) << endl; // 通过function对象调用封装的函数
    cout << (fun?"否":"是") << endl; // 现在不为空
    return 0;
}

```

##### lambda

**lambda表达式说明**：

lambda表达式（也称为闭包）是C++11引入的一种匿名函数对象，允许在代码中直接定义内联函数，具有以下特点：
- 可以在函数内部直接定义和使用
- 可以捕获外部变量（通过捕获列表）
- 语法简洁，避免了定义命名函数对象的繁琐
- 特别适合作为算法的谓词或回调函数

**基本语法**：
```cpp
[捕获列表](参数列表) mutable noexcept -> 返回类型 {
    函数体
}
```

**各部分说明**：
- **捕获列表**：指定如何捕获外部变量（值捕获[=]、引用捕获[&]、混合捕获等）
- **参数列表**：与普通函数参数列表类似，可选
- **mutable**：可选，允许修改值捕获的变量
- **noexcept**：可选，指定函数不抛出异常
- **-> 返回类型**：可选，显式指定返回类型，通常可由编译器推导
- **函数体**：lambda函数的实现代码

**捕获列表类型**：
- `[]`：不捕获任何外部变量
- `[=]`：以值方式捕获所有外部变量
- `[&]`：以引用方式捕获所有外部变量
- `[变量名]`：以值方式捕获指定变量
- `[&变量名]`：以引用方式捕获指定变量
- `[=, &变量名]`：默认以值方式捕获，除了指定的变量以引用方式捕获
- `[&, 变量名]`：默认以引用方式捕获，除了指定的变量以值方式捕获

**lambda表达式示例**：

```cpp
#include <iostream>
#include <functional>
sing namespace std;


int main()
{
    // 基本lambda表达式，无捕获
    auto add = [](int a, int b){
            return a + b;
    };
    cout << add(6, 8) << endl; // 调用lambda函数
    
    // 立即调用的lambda表达式（IIFE - Immediately Invoked Function Expression）
    int result = [](int a, int b)->int{ // 显式指定返回类型
        return a * b;
    }(4, 5); // 定义后立即调用，传递参数4和5
    cout << result << endl;
    
    return 0;
}

```

结果

![76225398838](C:\Users\魏士雄\AppData\Local\Temp\1762253988387.png)

**值捕获示例**：

```cpp
#include <iostream>
#include <functional>
sing namespace std;


int main()
{
    int x = 10, y = 20;
    // [=]表示以值方式捕获所有外部变量
    auto l1 = [=](){
        cout << x << " " << y << endl; // 值捕获的变量是只读的
        // x++ 这样的操作会导致编译错误，因为值捕获的变量默认是const的
    };
    l1();
    x = 40; // 修改原始变量
    l1(); // lambda中的值不会改变，仍然是捕获时的值
    return 0;
}

```

**引用捕获示例**：

```cpp
#include <iostream>
#include <functional>
sing namespace std;


int main()
{
    int x = 10, y = 20;
    // [&]表示以引用方式捕获所有外部变量
    auto l1 = [&](){
        cout << x << " " << y << endl;
        x++; // 可以修改引用捕获的变量的值
        y++;
    };
    l1();
    x = 40; // 修改原始变量
    l1(); // lambda中会反映原始变量的变化
    return 0;
}

```

**混合捕获示例**：

```cpp
#include <iostream>
#include <functional>
sing namespace std;


int main()
{
    int a = 1, b = 2, c = 3, d = 4;
    // a是以值方式捕获，b是以引用方式捕获，c和d不捕获
    auto l1 = [a, &b](){
        cout << a << b << endl;
        // a++ 这样的操作会导致编译错误
        b++; // 可以修改引用捕获的变量
    };
    l1();
    l1();
    l1();
    return 0;
}

```

**mutable关键字示例**：

```cpp
#include <iostream>
#include <functional>
sing namespace std;


int main()
{
    int count = 0;
    // mutable关键字允许修改值捕获的变量
    auto l1 = [count]() mutable {
        count++; // 在mutable lambda中可以修改值捕获的变量
        cout << count << endl;
        return count;
    };
    l1(); // 输出1
    l1(); // 输出2
    l1(); // 输出3
    // 注意：这里修改的是lambda内部的副本，不会影响外部的count变量
    return 0;
}
```
## 一、算法

### 1. 头文件分类

cpp

```
#include <algorithm>  // 最大的STL头文件，包含交换、查找、遍历、复制等算法
#include <numeric>    // 小型头文件，包含序列上的简单数学运算
```

### 2. 算法四大分类

| 分类               | 描述                             |
| ------------------ | -------------------------------- |
| **非可变序列算法** | 不直接修改容器内容的算法         |
| **可变序列算法**   | 可以修改容器内容的算法           |
| **排序算法**       | 对序列进行排序、合并、搜索等操作 |
| **数值算法**       | 对容器内容进行数值计算           |

### 3. 常用算法示例

cpp

```cpp
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
using namespace std;

void algorithm_examples() {
    vector<int> v{1, 3, 5, 5, 7, 9, 9, 9};
    
    // 相邻查找 - 返回第一对相邻相等元素的迭代器
    auto it = adjacent_find(v.begin(), v.end());
    if (it != v.end()) {
        cout << "第一个相邻重复元素是: " << *it << endl;
        cout << "位置: " << distance(v.begin(), it) << endl;
    }
    
    // 二分查找 (要求序列有序)
    if (binary_search(v.begin(), v.end(), 4)) {
        cout << "找到4" << endl;
    } else {
        cout << "没有4" << endl;
    }
    
    // 计数
    cout << "9出现的次数为: " << count(v.begin(), v.end(), 9) << endl;
    
    // for_each 遍历执行
    for_each(v.begin(), v.end(), [](int n) { cout << n << " "; });
    cout << endl;
    
    // 修改元素
    for_each(v.begin(), v.end(), [](int &n) { n *= 2; });
    
    // 随机重排
    random_device rd;
    mt19937 gen(rd());
    shuffle(v.begin(), v.end(), gen);
    
    // 排序
    sort(v.begin(), v.end());
}
```

## 二、设计模式

### 1. 设计模式分类

| 分类           | 用途             | 包含模式                           |
| -------------- | ---------------- | ---------------------------------- |
| **创建型模式** | 关注对象创建过程 | 单例、工厂、抽象工厂、建造者、原型 |
| **结构型模式** | 关注类和对象组合 | 适配器、装饰器、代理等             |
| **行为型模式** | 关注对象间通信   | 观察者、状态模式等                 |

### 2. 单例模式详解

#### 核心特点

- **唯一性**：整个应用程序中只有一个实例
- **全局访问**：通过统一接口访问实例
- **控制实例化**：构造函数私有化，防止外部直接创建

#### 饿汉式 (Eager Initialization)

cpp

```cpp
class Singleton {
private:
    static Singleton instance;  // 静态成员变量
    int data;
    
    // 构造函数私有化
    Singleton(int value) : data(value) {
        cout << "Singleton(int value)" << endl;
    }
    
public:
    // 禁止拷贝和赋值
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    
    // 提供公有静态访问方法
    static Singleton& getInstance() {
        return instance;
    }
    
    int getData() const { return data; }
    void setData(int value) { data = value; }
};

// 类外初始化 - 在程序开始时创建
Singleton Singleton::instance(10);
```

**特点**：类加载时创建，线程安全但可能浪费内存

#### 懒汉式 (Lazy Initialization)

cpp

```cpp
#include <mutex>

class Singleton {
private:
    static Singleton* instance;
    static mutex mtx;  // 互斥锁保证线程安全
    int data;
    
    Singleton(int value) : data(value) {
        cout << "Singleton(int value)" << endl;
    }
    
public:
    // 禁止拷贝和赋值
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    
    // 获取实例（双重检查锁定）
    static Singleton* getInstance() {
        if (instance == nullptr) {  // 第一次检查，避免不必要的加锁
            lock_guard<mutex> lock(mtx);
            if (instance == nullptr) {  // 第二次检查，确保线程安全
                instance = new Singleton(10);
            }
        }
        return instance;
    }
    
    // 释放资源
    static void destroy() {
        lock_guard<mutex> lock(mtx);
        if (instance != nullptr) {
            delete instance;
            instance = nullptr;
        }
    }
    
    int getData() const { return data; }
    void setData(int value) { data = value; }
};

// 类外初始化
Singleton* Singleton::instance = nullptr;
mutex Singleton::mtx;
```

**特点**：使用时创建，需要处理线程安全和内存释放

#### C++11 局部静态变量（推荐）

cpp

```cpp
class Singleton {
private:
    int data;
    
    Singleton(int value) : data(value) {
        cout << "Singleton(int value)" << endl;
    }
    
public:
    // 禁止拷贝和赋值
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    
    // C++11保证局部静态变量线程安全
    static Singleton& getInstance() {
        static Singleton instance(10);
        return instance;
    }
    
    int getData() const { return data; }
    void setData(int value) { data = value; }
};
```

## 三、智能指针

### 1. `auto_ptr` (已废弃)

```cpp
// C++98引入，C++17移除 - 不推荐使用
void auto_ptr_demo() {
    auto_ptr<Cat> c1(new Cat("Tom"));
    c1->speak();
    
    // 拷贝会转移所有权（危险！）
    auto_ptr<Cat> c2(c1);  // c1变为空
    // c1->speak();  // 错误！c1已为空
    c2->speak();
}
```

**问题**：拷贝语义不明确，容易误用

### 2. `unique_ptr` (推荐)

cpp

```cpp
#include <memory>

class Treasure {
private:
    string name;
public:
    Treasure(string name) : name(name) {
        cout << name << "获取宝藏" << endl;
    }
    ~Treasure() {
        cout << "失去宝藏" << endl;
    }
    void use() {
        cout << "使用宝藏" << endl;
    }
};

void unique_ptr_demo() {
    // 直接创建
    unique_ptr<Treasure> ptr1(new Treasure("华清"));
    ptr1->use();
    
    // 使用make_unique (C++14)
    auto ptr2 = make_unique<Treasure>("秘籍");
    ptr2->use();
    
    // 移动语义（转移所有权）
    unique_ptr<Treasure> ptr3 = move(ptr1);
    // ptr1->use();  // 错误！ptr1已转移
    ptr3->use();
    
    // 释放所有权
    Treasure* raw_ptr = ptr2.release();
    delete raw_ptr;
}
```

**特点**：

- 独占所有权，禁止拷贝
- 零额外开销
- 自动释放资源
- 支持移动语义

### 3. `shared_ptr`

cpp

```cpp
class Car {
private:
    string model;
public:
    Car(string model) : model(model) {
        cout << "购买新车: " << model << endl;
    }
    ~Car() {
        cout << "销毁车: " << model << endl;
    }
    void drive(string people) {
        cout << people << "在开" << model << endl;
    }
};

void shared_ptr_demo() {
    // 推荐使用make_shared（一次性分配）
    shared_ptr<Car> car1 = make_shared<Car>("宝马");
    cout << "引用计数: " << car1.use_count() << endl;  // 1
    
    // 共享所有权
    shared_ptr<Car> car2 = car1;
    cout << "引用计数: " << car1.use_count() << endl;  // 2
    
    car1->drive("张三");
    car2->drive("李四");
    
    // 离开作用域自动释放
}
```

**特点**：

- 共享所有权，引用计数管理
- `make_shared`一次性分配，效率更高
- 存在循环引用风险

### 4. `weak_ptr` - 解决循环引用

cpp

```cpp
class Node {
public:
    int data;
    weak_ptr<Node> next;  // 使用weak_ptr打破循环引用
    
    Node(int value) : data(value) {
        cout << "构造节点: " << data << endl;
    }
    ~Node() {
        cout << "销毁节点: " << data << endl;
    }
};

void weak_ptr_demo() {
    shared_ptr<Node> n1 = make_shared<Node>(1);
    shared_ptr<Node> n2 = make_shared<Node>(2);
    
    n1->next = n2;  // 不增加n2的引用计数
    n2->next = n1;  // 不增加n1的引用计数
    
    // 使用weak_ptr访问对象
    if (auto sp = n1->next.lock()) {
        cout << "通过lock访问: " << sp->data << endl;
    } else {
        cout << "节点已被销毁" << endl;
    }
}
```

**特点**：

- 解决`shared_ptr`循环引用问题
- 不增加引用计数
- 需要通过`lock()`获取可用的`shared_ptr`

## 四、最佳实践总结

### 算法使用原则

1. **理解复杂度**：选择适合时间复杂度的算法
2. **正确分类**：根据需求选择可变/不可变算法
3. **迭代器安全**：注意算法对迭代器的影响

### 单例模式选择

| 场景             | 推荐方案            |
| ---------------- | ------------------- |
| 简单应用         | C++11局部静态变量   |
| 需要控制创建时机 | 懒汉式+双重检查锁定 |
| 性能敏感         | 饿汉式              |

### 智能指针使用指南

1. **默认选择**：优先使用`unique_ptr`
2. **共享资源**：需要共享时使用`shared_ptr`
3. **打破循环**：循环引用时使用`weak_ptr`
4. **创建方式**：优先使用`make_shared`和`make_unique`
5. **避免混合**：不要混合使用智能指针和裸指针

### 内存管理原则

- **RAII**：资源获取即初始化
- **所有权明确**：清晰定义资源所有权
- **异常安全**：利用智能指针保证异常安全
- **及时释放**：合理管理对象生命周期


```cpp
#include <iostream>

using namespace std;

template <typename T>
class MyShareptr
{
private:
    T *ptr;
    long *countRof;
protected:
    void release()
    {
        (*countRof)--;
        if (*countRof == 0)
        {
            if (ptr != NULL)
            {
                delete ptr;
                ptr = nullptr;
            }
            delete countRof;
            countRof = nullptr;
        }
    }
public:
    explicit MyShareptr(T *ptr = NULL):ptr(ptr),countRof(new long(1)){}
    MyShareptr(const MyShareptr<T> &other):ptr(other.ptr),countRof(other.countRof)
    {
        (*countRof)++;
    }
    ~MyShareptr()
    {
        release();
    }
    MyShareptr<T> operator= (const MyShareptr<T> &other)
    {
        if (this != &other)
        {
            release();
            ptr = other.ptr;
            countRof = other.countRof;
            (*countRof)++;
        }
    }
    T& operator* ()
    {
        return *ptr;
    }
    T&operator-> ()
    {
        return ptr;
    }
    T* Getptr()
    {
        return ptr;
    }
    int Getcount()
    {
        return *countRof;
    }
};

int main()
{
    MyShareptr<int> p1(new int(10));
    cout << *p1 << endl;
    cout << p1.Getcount() << endl;
    MyShareptr<int> p2 = p1;
    cout << p1.Getcount() << endl;
    cout << *p2 << endl;
    return 0;
}

```

#### const_cast

```cpp
#include <iostream>

using namespace std;

void print(char *p)
{
    cout << p << endl;
}

int main()
{
    char a[] = "hello";
    const char *cp = a;
    print(const_cast<char *>(cp));
    return 0;
}

```

#### static_cast

```cpp
    int a = 7;
    double b = static_cast<double>(a) / 2;
    int c = static_cast<int>(b);
    cout << b << " " << c << endl;
```

#### dynamic_cast

```cpp
#include <iostream>

using namespace std;

class Base
{
public:
    virtual ~Base(){}
};

class Derived : public Base
{

};


int main()
{
    Base *bp = new Derived;
    if (Derived *dp = dynamic_cast<Derived*>(bp))
    {
        cout << "转换成功" << endl;
    }
    else{
        cout << "转换失败" << endl;
    }

    return 0;
}

```

#### reinterpret_cast

```cpp
    int *p = new int(10);
    uintptr_t p_addr = reinterpret_cast<uintptr_t>(p);
    cout << hex << p_addr << endl;
```
#### constexpr

```cpp
void func(int a)
{
    int N = a;
    int arr[N];
    constexpr int N2 = 5;
    int arr1[N2];
//    在编译阶段无法计算出N3
//    constexpr int N3 = a;
}

```

心型

```cpp
#include <iostream>
#include <cmath>
using namespace std;


int main() {
    int i, j;
    double x, y;
    for (i = 0; i <= 30; i++)
    {
        for (j = 0; j <= 60; j++)
        {
            x = (j - 30) * 0.1;
            y = (15 - i) * 0.1;
            double v = pow(0.2 * x * x + y * y - 1, 3) - 0.2 * x * x * pow(y, 3);
            if (v <= 0)
            {
                printf("\033[41m*\033[0m");
            }else
            {
                printf(" ");
            }
        }
        printf("\n");
    }
    return 0;
}

```

