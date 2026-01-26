#ifndef ORDER_H
#define ORDER_H

#include <string>

class Order {
public:
    Order(int id, std::string side, double price, int qty);
    
    int getRemaining() const;
    void fill(int qty);
    bool isFilled() const;
    void print() const;
    
private:
    int order_id;
    std::string side;
    double price;
    int quantity;
    int filled;
};

#endif // ORDER_H