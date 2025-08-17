#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

template <typename T, size_t S>
class CircularBuffer {
private:
    T buffer[S];
    size_t head = 0;
    size_t tail = 0;
    size_t count = 0;

public:
    CircularBuffer() {}

    void push(const T& item) {
        buffer[head] = item;
        head = (head + 1) % S;
        
        if (count == S) {
            tail = (tail + 1) % S;
        } else {
            count++;
        }
    }

    T shift() {
        if (isEmpty()) {
            return T();
        }
        
        T item = buffer[tail];
        tail = (tail + 1) % S;
        count--;
        return item;
    }

    T& operator[](size_t index) {
        return buffer[(tail + index) % S];
    }

    const T& operator[](size_t index) const {
        return buffer[(tail + index) % S];
    }

    bool isEmpty() const {
        return count == 0;
    }

    bool isFull() const {
        return count == S;
    }

    size_t size() const {
        return count;
    }

    size_t capacity() const {
        return S;
    }

    void clear() {
        head = 0;
        tail = 0;
        count = 0;
    }
};

#endif // CIRCULAR_BUFFER_H