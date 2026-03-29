import org.antlr.v4.runtime.*;
import java.nio.file.Files;
import java.nio.file.Paths;

public class Benchmark {
    public static void main(String[] args) throws Exception {
        if (args.length < 2) {
            System.err.println("Usage: java Benchmark <input_file> <iterations>");
            System.exit(1);
        }
        
        String source = new String(Files.readAllBytes(Paths.get(args[0])));
        int iterations = Integer.parseInt(args[1]);
        long tokensCount = 0;
        
        long startTime = System.currentTimeMillis();
        
        for (int i = 0; i < iterations; i++) {
            CloudPolLexer lexer = new CloudPolLexer(CharStreams.fromString(source));
            while (true) {
                Token tok = lexer.nextToken();
                if (tok.getType() == Token.EOF) break;
                tokensCount++;
            }
        }
        
        long endTime = System.currentTimeMillis();
        
        System.out.println("ANTLR (Java) Parsed " + tokensCount + " tokens in " + iterations + " iterations.");
        System.out.println("Time taken: " + (endTime - startTime) + " ms");
    }
}
