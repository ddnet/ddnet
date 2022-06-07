#include <iostream>

int main(int argc, char** argv)
{
    int m_aTest[512];
    decltype(m_aTest) a;
    int *retest[512];
    decltype(retest) b;
    int m_aaTest[10][20];
    decltype(m_aaTest) c;
    int *m_aapTest[10][20];
    decltype(m_aapTest) d;

    std::cout << typeid(decltype(m_aTest)).name() << std::endl;
    std::cout << sizeof(a) << std::endl;
    std::cout << typeid(std::remove_pointer<decltype(m_aTest)>::type).name() << std::endl << std::endl;

    std::cout << typeid(decltype(retest)).name() << std::endl;
    std::cout << sizeof(b) << std::endl;
    std::cout << typeid(std::remove_pointer<decltype(retest)>::type).name() << std::endl << std::endl;

    std::cout << typeid(decltype(m_aaTest)).name() << std::endl;
    std::cout << sizeof(c) << std::endl;
    std::cout << typeid(std::remove_pointer<decltype(m_aaTest)>::type).name() << std::endl << std::endl;

    std::cout << typeid(decltype(m_aapTest)).name() << std::endl;
    std::cout << sizeof(d) << std::endl;
    std::cout << typeid(std::remove_pointer<decltype(m_aapTest)>::type).name() << std::endl << std::endl;

    std::cout << &m_aTest << std::endl << m_aTest << std::endl;
    std::cout << typeid(decltype(&m_aTest)).name() << std::endl;
    std::cout << sizeof(int) << std::endl;

    return 0;
}
