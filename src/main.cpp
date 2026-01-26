#include "order.h"
#include <iostream>

int main() {
    std::cout << "Order Book Engine v0.1" << std::endl;
    
    Order order(1, "BUY", 150.50, 100);
    order.print();
    
    return 0;
}