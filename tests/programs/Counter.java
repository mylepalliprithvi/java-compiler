public class Counter {
    int value;

    Counter(int start) {
        this.value = start;
    }

    public int get() {
        return this.value;
    }

    public void bump() {
        this.value = this.value + 1;
    }

    public static void main(String[] args) {
        Counter c = new Counter(10);
        c.bump();
        c.bump();
        c.bump();
        System.out.println(c.get());
        System.out.println(c.value);
    }
}
