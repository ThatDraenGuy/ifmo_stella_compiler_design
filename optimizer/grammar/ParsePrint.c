/* Minimal BNFC/Bison AST parse/print example using StellaIR.cf.
 * BNFC line comments stop at LF. Normalise CR line endings on verified input.
 * Raw CR inside a JSON string is invalid and belongs to the decoding checks.
 */
#include <stdio.h>
#include "Absyn.h"
#include "Parser.h"
#include "Printer.h"

int main(void)
{
  int ch;
  Unit unit;
  FILE *normal = tmpfile();
  if (!normal) { perror("tmpfile"); return 1; }
  while ((ch = getchar()) != EOF) {
    if (fputc(ch == '\r' ? '\n' : ch, normal) == EOF) {
      perror("write"); fclose(normal); return 1;
    }
  }
  if (ferror(stdin) || fflush(normal) != 0 || fseek(normal, 0, SEEK_SET) != 0) {
    perror("input"); fclose(normal); return 1;
  }
  unit = pUnit(normal);
  fclose(normal);
  if (!unit) return 1;
  puts(printUnit(unit));
  free_Unit(unit);
  return ferror(stdout) ? 1 : 0;
}
