int fibonacci(int n) {
    if (n <= 1) {
        return n;
    }

    const auto v1 = fibonacci(n - 1);

    println("fibonacci(" + (n - 1) + "): " + v1);
    sleep(100);

    const auto v2 = fibonacci(n - 2);

    println("fibonacci(" + (n - 2) + "): " + v2);
    sleep(100);

    return v1 + v2;
}

void main() {
    const auto n = 50;
    const auto result = fibonacci(n);
    println("result: fibonacci(" + n + "): " + result);
}
