-- Minimal BNFC parse/print example using the generated AST and printer.
module Main (main) where
import StellaIR.Par (pUnit, myLexer)
import StellaIR.Print (printTree)
import System.Exit (die)
import System.IO (stdin, stdout, hSetEncoding, utf8)
main :: IO ()
main = do
  hSetEncoding stdin utf8
  hSetEncoding stdout utf8
  getContents >>= either die (putStrLn . printTree) . pUnit . myLexer . map (\c -> if c == '\r' then '\n' else c)
