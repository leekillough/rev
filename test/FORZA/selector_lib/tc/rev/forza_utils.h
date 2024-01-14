#include <fcntl.h>
#include <string.h>
#include "../../../../../common/syscalls/syscalls.h" 


// #define THREADS 1
int THREADS;
uint64_t TOTAL_THREADS;
volatile int *fbarriers;
#define BUF_SIZE 50000
#define MAX_PRINT_ARGS 5
#define PKT_QUEUE_SIZE 100000

typedef unsigned long int forza_thread_t;
typedef int fd_t;

volatile void *print_args[MAX_PRINT_ARGS];

// #define FDADDRESS(fd) ((char *) ((0x1100L) | (fd << 3)))

char revbuf[4096] = {0};
char revbufhead = 0;

void *forza_malloc(size_t size)
{
    return (void*) rev_mmap(0,                 
          size,
          PROT_READ | PROT_WRITE | PROT_EXEC, 
          MAP_PRIVATE | MAP_ANONYMOUS, 
          -1,                   
          0);
}

void* operator new(std::size_t t)
{
    void* p = forza_malloc(t);
    return p;
}

// void operator delete(void * p)
// {
//     forza_free(p);
// }


void forza_putc(fd_t fd, char c)
{
    // putc(c, stderr);
    // rev_write(STDOUT_FILENO, &c, 1);
    revbuf[revbufhead] = c;
    revbufhead++;
}

// char forza_getc(fd_t fd)
// {
//     return *(FDADDRESS(fd));
// }

void forza_puts(fd_t fd, char * s) 
{
    char c;
    if (!s)
    {
        return;
    }
    while (c = *s++)
    {
        forza_putc(fd, c);
    }
}


#define MAX_PRECISION	(10)
static const double rounders[MAX_PRECISION + 1] =
{
	0.5,				// 0
	0.05,				// 1
	0.005,				// 2
	0.0005,				// 3
	0.00005,			// 4
	0.000005,			// 5
	0.0000005,			// 6
	0.00000005,			// 7
	0.000000005,		// 8
	0.0000000005,		// 9
	0.00000000005		// 10
};

void printfloat(fd_t fd, double f)//, int precision), can add precision back later
{
    char buf [40];
	char * ptr = buf;
	char * p = ptr;
	char * p1;
	char c;
	long intPart;
    int precision = MAX_PRECISION;

	// check precision bounds
	if (precision > MAX_PRECISION)
		precision = MAX_PRECISION;

	// sign stuff
	if (f < 0)
	{
		f = -f;
		*ptr++ = '-';
	}

	// if (precision < 0)  // negative precision == automatic precision guess
	// {
	// 	if (f < 1.0) precision = 6;
	// 	else if (f < 10.0) precision = 5;
	// 	else if (f < 100.0) precision = 4;
	// 	else if (f < 1000.0) precision = 3;
	// 	else if (f < 10000.0) precision = 2;
	// 	else if (f < 100000.0) precision = 1;
	// 	else precision = 0;
	// }

	// round value according the precision
	if (precision)
		f += rounders[precision];

	// integer part...
	intPart = f;
	f -= intPart;

	if (!intPart)
		*ptr++ = '0';
	else
	{
		// save start pointer
		p = ptr;

		// convert (reverse order)
		while (intPart)
		{
			*p++ = '0' + intPart % 10;
			intPart /= 10;
		}

		// save end pos
		p1 = p;

		// reverse result
		while (p > ptr)
		{
			c = *--p;
			*p = *ptr;
			*ptr++ = c;
		}

		// restore end pos
		ptr = p1;
	}

	// decimal part
	if (precision)
	{
		// place decimal point
		*ptr++ = '.';

		// convert
		while (precision--)
		{
			f *= 10.0;
			c = f;
			*ptr++ = '0' + c;
			f -= c;
		}
	}

	// terminating zero
	*ptr = 0;

	forza_puts(fd, buf);
}


void printint(int fd, int xx, int base, int sgn)
{
  static char digits[] = "0123456789ABCDEF";
  char buf[30];
  int64_t i, neg;
  uint64_t x;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);
  if(neg)
    buf[i++] = '-';

  while(--i >= 0)
    forza_putc(fd, buf[i]);
}

void forza_fprintf(fd_t fd, const char * fmt, volatile void **args) {
    char c;
    size_t i;
    int state = 0;
    volatile void **idx = args;
    
    if (fmt == NULL)
    {
        return;
    }

    for (i = 0; fmt[i]; i++) {
        c = fmt[i] & 0xFF;
        if (state == 0) {
            if (c == '%') {
                state = '%';
            } else {
                forza_putc(fd, c);
            }
        } else {
            state = 0;
            if (c == 'd') {
                printint(fd, *((int *)(*idx)), 10, 1);
                idx++;
            } else if (c == 'x') {
                printint(fd, *((int *)(*idx)), 16, 0);
                idx++;
            } else if (c == 'p') {
                printint(fd, *((uintptr_t *)(*idx)), 16, 0);
                idx++;
            } else if (c == 's') {
                char *s = *((char  **)(*idx));
                idx++;
                forza_puts(fd, s);
            } else if (c == 'c') {
                char val = *((char  *)(*idx));
                idx++;
                forza_putc(fd, val);
            } else if (c == 'l') {
                //this is not how long really works, need to reimplement for print codes longer than 1 char
                printint(fd, *((long *)(*idx)), 10, 1); //print the long, base 10, signed
                idx++;
            }  else if (c == 'f') { // double precision only
                printfloat(fd, *((double *)(*idx))); //print the long, base 10, signed
                idx++;
            }else if (c == '%') {
                forza_putc(fd, c);
            } else {
                //unsupported %
                forza_putc(fd, '%');
                forza_putc(fd, c);
            }
        }
    }
    
    revbuf[revbufhead++] = '\0';
    rev_write(STDOUT_FILENO, revbuf, revbufhead);

    //clear out the revbuf
    for(int clear = 0; clear < revbufhead; clear++)
    {
        revbuf[clear] = 0;
    }
    revbufhead = 0;
}
