public class Fib {
    public static void main(String[] args) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < 10) {
            System.out.println(a);
            int next = a + b;
            a = b;
            b = next;
            i = i + 1;
        }
        for (int j = 0; j < 5; j = j + 1) {
            System.out.println(j * j);
        }
    }
}
