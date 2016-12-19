# xopen #

GNU/Linux command-line program to execute a predifined command based on given
files' extension.
(Just check out [mimeopen](http://search.cpan.org/dist/File-MimeInfo/mimeopen)...)

## Installation ##

```git clone https://github.com/Barbuseries/xopen
cd xopen/code
make
```

Adding the executable to your ```$BIN_HOME``` must be done manually
(will probably be automated later on).

## Configuration ##

To use xopen, you must first add *instructions* inside it's config file
(usually ```~/.config/xopen.conf```).

Each instruction follow the syntax:
```CMD - EXTENSION [EXTENSION ...]```
where ```CMD``` is a command to be executed, and ```EXTENSION``` is
one of the extensions a file's extension must be equal to to match the instruction.
If a line only contains ```CMD```, it's the default instruction
(executed when no other instruction matches).

### NOTE ###

```EXPRESSION``` is dot-less.

```CMD``` must be recognised by ```which``` (shell functions won't work).
