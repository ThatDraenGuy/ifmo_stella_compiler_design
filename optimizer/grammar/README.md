# Грамматики Stella IR

Выберите один маршрут или JSON. Входы задания заранее проверены `stella ir-check`; семантический верификатор писать не требуется. Проверяйте результат своего прохода той же командой.

Команды выполняются из корня стартового набора. `JsonString` содержит кавычки и escape-последовательности JSON, а `Decimal` сохраняет текст числа. Декодируйте их библиотекой JSON и целочисленными операциями своего языка. Все операнды IR перечислены явно; статические атрибуты не содержат скрытых локальных значений.

## BNFC, Haskell

Проверены BNFC 2.9.6.1, Alex 3.5.4.0, Happy 2.0.2 и GHC 9.10.3. Генератор даёт AST, парсер и принтер:

```sh
bnfc --haskell -d -m -o build-hs grammar/StellaIR.cf
make -C build-hs
ghc -ibuild-hs -outputdir build-hs/objects \
  grammar/ParsePrint.hs -o build-hs/parse-print
build-hs/parse-print < examples/swap.sir
```

BNFC поддерживает и другие целевые языки; здесь проверен Haskell.

## Bison и Flex, C

Проверены Bison 3.8.2, Flex 2.6.4 и Clang. Старый Bison 2.3 из macOS не подходит. Файлы C сгенерированы из того же `StellaIR.cf`:

```sh
mkdir build-c
cp grammar/bison/* grammar/ParsePrint.c build-c/
cd build-c
bison -pstella_ir_ StellaIR.y -o Parser.c
flex -Pstella_ir_ -oLexer.c StellaIR.l
cc -std=gnu99 -I. ParsePrint.c Absyn.c Buffer.c Lexer.c Parser.c Printer.c -o parse-print
./parse-print < ../examples/swap.sir
```

`Parser.h` объявляет `pUnit(FILE *)` и `psUnit(const char *)`; `Absyn.h` — AST, `Printer.h` — принтер. Предупреждение об устаревшем `%pure_parser` допустимо; конфликтов грамматики быть не должно.

## ANTLR, Java

Проверены ANTLR 4.13.2 и Java 17. Задайте `ANTLR_JAR` абсолютным путём к официальному `antlr-4.13.2-complete.jar`:

```sh
mkdir build-java
cp grammar/StellaIR.g4 grammar/ParsePrint.java build-java/
cd build-java
java -jar "$ANTLR_JAR" -Dlanguage=Java StellaIR.g4
javac -cp "$ANTLR_JAR" *.java
java -cp ".:$ANTLR_JAR" ParsePrint < ../examples/swap.sir
```

Пример строит дерево разбора и печатает его токены. Для преобразований используйте visitor/listener или свой AST. Грамматика не содержит действий на языке реализации; другие целевые языки ANTLR здесь не проверены.

Грамматики и примеры чтения/печати предоставлены проектом Stella; условия использования — в `LICENSE` в корне набора. Это готовые компоненты, не часть самостоятельной реализации алгоритмов этапа 2.
