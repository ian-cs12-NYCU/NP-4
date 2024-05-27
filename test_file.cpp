#include <iostream>
#include <vector>
#include <bitset>

void printBinary(const std::vector<unsigned char>& data) {
    for (unsigned char byte : data) {
        std::bitset<8> bits(byte); // 将字节转换为 bitset
        std::cout << bits << " ";
    }
    std::cout << std::endl;
}

int main() {
    // 示例数据，假设 data_ 数组包含从 SOCKS 请求读取的字节
    std::vector<unsigned char> data_ = {0x04, 0x01, 0x07, 0xD0, 0x7F, 0x00, 0x00, 0x01}; // SOCKS request example

    // 输出数据的二进制表示
    std::cout << "Binary representation of the SOCKS request:" << std::endl;
    printBinary(data_);

    return 0;
}