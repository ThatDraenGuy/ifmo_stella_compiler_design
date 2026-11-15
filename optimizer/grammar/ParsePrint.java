// Minimal ANTLR parse/print example. Token text preserves JSON escape spelling.
// Follow ANTLR's standard lexer -> token stream -> parser API.
import org.antlr.v4.runtime.*;
import java.nio.charset.StandardCharsets;

public final class ParsePrint {
  private static final BaseErrorListener FAIL = new BaseErrorListener() {
    @Override public void syntaxError(Recognizer<?, ?> r, Object symbol, int line,
        int column, String message, RecognitionException error) {
      throw new IllegalArgumentException(line + ":" + column + ": " + message);
    }
  };
  public static void main(String[] args) throws Exception {
    try {
      StellaIRLexer lexer = new StellaIRLexer(CharStreams.fromStream(System.in));
      lexer.removeErrorListeners(); lexer.addErrorListener(FAIL);
      CommonTokenStream tokens = new CommonTokenStream(lexer);
      StellaIRParser parser = new StellaIRParser(tokens);
      parser.removeErrorListeners(); parser.addErrorListener(FAIL);
      parser.unit();
      StringBuilder output = new StringBuilder();
      for (Token token : tokens.getTokens()) {
        if (token.getType() != Token.EOF && token.getChannel() == Token.DEFAULT_CHANNEL)
          output.append(token.getText()).append(' ');
      }
      System.out.write((output.toString() + "\n").getBytes(StandardCharsets.UTF_8));
    } catch (IllegalArgumentException error) {
      System.err.println(error.getMessage()); System.exit(1);
    }
  }
}
