#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>


/*Load a file into RAM
 * should be zero terminated
 */
int load_file(const char *fn, uint8_t **data)
{
	if (fn==NULL) return -1;
	FILE *f=fopen(fn, "r");
	if (f==NULL) return -1;
	fseek(f, 0L, SEEK_END);
	long fs = ftell(f);
	if (fs<=0) return -1;
	if (fs>0xffffff) return -1;
	*data=malloc(fs+1);
	memset(*data, 0, fs+1);
	fseek(f, 0L, SEEK_SET);
	fread(*data, fs, 1, f);
	fclose(f);
	return fs;
}

typedef struct vpde_s{
	struct vpde_s *previous;
	struct vpde_s *next;
	uint8_t t;
	uint8_t *data;
	int len;
} vdpe_t;

vdpe_t *add_vdpe(vdpe_t *v, uint8_t *data, int len)
{
	if (len<0) return v;
	if (data==NULL) return v;
	vdpe_t *x=malloc(sizeof(vdpe_t));
	if (x==NULL) return v;
	memset(x, 0, sizeof(vdpe_t));
	x->previous=v;
	x->next=NULL;
	x->data=data;
	x->len=len;
	if (v!=NULL) {
		v->next=x;
	}
	return x;
}

vdpe_t *find_vdpe(uint8_t *data, int len)
{
	vdpe_t *v=NULL;
	int start=0;
	int n;
	for (n=0; n<len; n++) {
		if (data[n]==0x1f) {
			v=add_vdpe(v, &(data[start]), n-start);
			start=n;
		}
	}
	v=add_vdpe(v, &(data[start]), len-start);
	return v;
}


vdpe_t *first_vdpe(vdpe_t *s)
{
	if (s==NULL) return NULL;
	if (s->previous==NULL) return s;
	return first_vdpe(s->previous);
}


void print_dcrs(uint8_t *d, int len)
{
	if (len<=0) return;
	if (d==NULL) return;
	printf("DCRS ");
	if ((d[0]==0x20) && ((d[1]&0xf0)==0x40) && ((d[2]&0xf0)==0x40)) {
		int p=d[1]&0x0f;
		int q=d[2]&0x0f;
		if (p==0x6) printf("12x12 ");
		if (p==0x7) printf("12x10 ");
		if (p==0xA) printf(" 6x12 ");
		if (p==0xB) printf(" 6x10 ");
		if (p==0xC) printf(" 6x 5 ");
		if (p==0xF) printf(" 6x 6 ");
		if (q==0x1) printf(" 2 Farben ");
		if (q==0x2) printf(" 4 Farben ");
		if (q==0x4) printf("16 Farben ");
	}
	if (d[1]==0x30) {
		printf("character: %c (%02x) \t", d[0], d[0]);
	}
	int n;
	for (n=0; n<len; n++) {
		printf("%02x ", d[n]);
	}
	printf("\n");
}


typedef struct {
	int g[4];
	int left;
	int right;
}terminal_state_t;

void init_terminal_state(terminal_state_t *t)
{
	int n;
	for (n=0; n<4; n++) t->g[n]=n;
	t->left=0;
	t->right=2;
}

void print_apa(const uint8_t *d, const int len, terminal_state_t *t)
{
	if (d==NULL) return;
	if (len<=0) return;
	if (d[0]==0x08) {
		printf("APB (left)\n");
		return print_apa(&(d[1]), len-1, t);
	}
	if (d[0]==0x09) {
		printf("APF (right)\n");
		return print_apa(&(d[1]), len-1, t);
	}
	if (d[0]==0x0A) {
		printf("APD (down)\n");
		return print_apa(&(d[1]), len-1, t);
	}
	if (d[0]==0x0B) {
		printf("APU (up)\n");
		return print_apa(&(d[1]), len-1, t);
	}
	if (d[0]==0x0C) {
		printf("CLEAR SCREEN\n");
		return print_apa(&(d[1]), len-1, t);
	}
	if (d[0]==0x0D) {
		printf("APR (return)\n");
		return print_apa(&(d[1]), len-1, t);
	}
	if (d[0]==0x0e) {
		printf("G1 to left page %d\n", t->g[1]);
		t->left=t->g[1];
		return print_apa(&(d[1]), len-1, t);
	}
	if (d[0]==0x0f) {
		printf("G0 to left page %d\n", t->g[0]);
		t->left=t->g[0];
		return print_apa(&(d[1]), len-1, t);
	}
	if (d[0]==0x12) {
		printf("REPEAT %d\n", d[1]-0x40);
		return print_apa(&(d[2]), len-2, t);
	}

	if ( (len>=4) && (d[0]==0x1b) && (d[2]==0x20) && (d[3]==0x40) ){
		int p=d[1]-0x28;
		printf("DCRS into G%d\n", p);
		return print_apa(&(d[4]), len-4, t);
	}
	if (d[0]==0x1b) { //Escape
		int a=d[1];
		int b=d[2];
		if ((d[1]==0x22) && (d[2]==0x40)) {
			printf("SER\n");
			return print_apa(&(d[3]), len-3, t);
		}
		if ((d[1]==0x22) && (d[2]==0x41)) {
			printf("PAR\n");
			return print_apa(&(d[3]), len-3, t);
		}
		if ((d[1]==0x23) && (d[2]==0x20)) {
			printf("ganze Zeile ");
			return print_apa(&(d[3]), len-3, t);
		}
		if ((d[1]==0x23) && (d[2]==0x21)) {
			printf("ganzer Schirm ");
			return print_apa(&(d[3]), len-3, t);
		}
		if ( (a>=0x28) && (a<=0x2b) ) {
			int page=-1;
			if (b==0x40) page=0; else
			if (b==0x63) page=1; else
			if (b==0x62) page=2; else
			if (b==0x64) page=3; else
			if ( (b==0x20) && (d[3]==0x40) ) page=4; else
				printf("Unknown %02x in", a);
			int to=a-0x28;
			t->g[to]=page;
			printf("G%d->G%d\n", page, to);
			if (page==4) return print_apa(&(d[4]), len-4, t);
		return print_apa(&(d[3]), len-3, t);
		}
	}
	if ((d[0]>=0x80) && (d[0]<=0x9f) && (d[0]!=0x9b)){
		if ((d[0]>=0x80) && (d[0]<=0x87) ) {
			printf("FG %d\n", d[0]-0x80);
		} else
		if ((d[0]>=0x90) && (d[0]<=0x97) ) {
			printf("BG %d\n", d[0]-0x90);
		} else printf("UNKONWN %02x\n", d[0]);
		return print_apa(&(d[1]), len-1, t);
	}
	if ((d[0]&0x7f)<0x20) {
		printf("UNNKONW %02x\n", d[0]);
		return print_apa(&(d[1]), len-1, t);
	}
	printf("TXT \"");
	int n;
	for (n=0; n<len; n++) {
		uint8_t c=d[n];
		int high=(c>>7);
		int low=c&0x7f;
		if (low<0x20) {
			printf("\"\n");
			return print_apa(&(d[n]), len-n, t);
		}
		int page=-1;
		if (high==0) page=t->left;
		if (high==1) page=t->right;
		if (page==0) {
			printf("%c", low);
		} else printf("&G%d-%02x;",page, c);
	}
	printf("\"\n");
}


void print_vdpe(const vdpe_t *v, terminal_state_t *ts)
{
	if (v==NULL) return;
	uint8_t t=v->data[1];
	if  ( (t==0x20) || (t==0x21) ) {
		printf("TFI\n");
	} else
	if (t==0x23) {
		print_dcrs(&(v->data[2]), v->len-2);
	} else 
	if (t==0x26) {
		printf("Define Colour\n");
	} else
	if (t==0x2d) {
		printf("Define Format\n");
	} else
	if (t==0x2e) {
		printf("Timing Control\n");
	} else 
	if (t==0x2f) {
		printf("Reset\n");
	} else
	if ( (t>=0x41) && (t<=0x41+40) )
	{
		printf("APA %02d %02d\n", t-0x41, v->data[2]);
		print_apa(&(v->data[3]), v->len-3, ts);
		printf("\n");
	}
}

void print_vdpes(const vdpe_t *v, terminal_state_t *t)
{
	if (v==NULL) return;
	print_vdpe(v, t);
	return print_vdpes(v->next,t);
}



int main(int argc, char *argv[])
{
	if (argc!=2) return 1;	
	uint8_t *data=NULL;
	int len=load_file(argv[1],&data);
	printf("len=%d\n", len);
	vdpe_t *v=find_vdpe(data, len);
	vdpe_t *first=first_vdpe(v);
	terminal_state_t t;
	init_terminal_state(&t);
	print_vdpes(first,&t);
	return 0;
}
