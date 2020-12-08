SRCDIR = ./src
BINDIR = ../bin

all:
	@make -s -C $(SRCDIR) all
	@make -s -C $(SRCDIR) clean-obj

clean:
	@make -s -C $(SRCDIR) clean

clean-obj:
	@make -s -C $(SRCDIR) clean-obj

.NOEXPORT:
