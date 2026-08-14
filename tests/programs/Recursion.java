public class Recursion {
    int factorial(int n) {
        if (n <= 1) {
            return 1;
        } else {
            return n * factorial(n - 1);
        }
    }

    public static void main(String[] args) {
        Recursion r = new Recursion();
        System.out.println(r.factorial(0));
        System.out.println(r.factorial(1));
        System.out.println(r.factorial(5));
        System.out.println(r.factorial(10));
    }
}
