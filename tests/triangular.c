int triangular (int n) {
    int result = 0;

    while (n) {
        result = result + n;
        n--;
    }

    return result;
}

int main () {
    return triangular(5);
}
