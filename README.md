Small utility that helps to view and edit bytes in binary files from command line.

```
usage:
  simple-hexedit --help
  simple-hexedit r[ead] [<offset> <length>] <file>
  simple-hexedit w[rite] <offset> <data> <file>
  simple-hexedit m[emset] <offset> <length> <char> <file>


  read, write, memset: <offset> and <length> may be hexadecimal prefixed with
    `0x' or `\x', an octal number prefixed with `0' or decimal

  read: <length> set to 0 or `all' will print all bytes

  write, memset: <offset> set to `append' will write data directly after the
    end of the file

  write: <data> must be hexadecimal without prefixes (whitespaces are ignored)

  write: <char> can be a literal character, escaped control character,
    hexadecimal value prefixed with `0x' or `\x', an octal number prefixed
    with `0' or a decimal number prefixed with `\'
```
