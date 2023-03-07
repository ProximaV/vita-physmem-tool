#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define MAJVER 3
#define MINVER 0

typedef struct {
	int nextblock;
	int flags;
	int rectype;
	unsigned int va;
	int length;
	int unknown;
	unsigned int pa;
	unsigned int memtype;
} memblock;

unsigned char * g_ptr;
char *g_infilename=0;
unsigned int g_ttbr0=0;
unsigned int g_ttbr1=0;
unsigned int g_ttbcr=0;
unsigned int g_base=0;
unsigned int g_start=0;
unsigned int g_end=0;
int g_print_only=0;
char *g_exfilename=0;

unsigned char * g_outbuf=0;

HANDLE hFile, hMap;

FILE *open_file(char *name, int *size)
{
	FILE *fp;
	int res;

	res = fopen_s(&fp,name, "rb");
	if(res) {
		return NULL;
	}
	if(fp==NULL){
		//printf("Open file %s failed!\n", name);
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	*size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	return fp;
}						

unsigned char *load_file(char *name, int *size)
{
	FILE *fp;
	unsigned char *buf;

	fp = open_file(name, size);
	if(fp==NULL)
		return NULL;
	buf = (unsigned char *) malloc(*size);
	if(buf)	fread(buf, *size, 1, fp);
	fclose(fp);

	return buf;
}

int write_file(char *file, void *buf, int size)
{
	FILE *fp;
	int written;
	int res;
	
	res = fopen_s(&fp,file, "wb");
	if(res)
		return -1;
	written = fwrite(buf, 1, size, fp);
	fclose(fp);

	return written;
}

unsigned char * open_source_map(char * filename, int *filesize) {

	int eboot_size_upper=0;
	unsigned char * fileptr=0;
	hFile=CreateFile((LPCWSTR) filename, GENERIC_READ,
                           FILE_SHARE_READ,
                           NULL,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           NULL);
	if(!hFile) {
		printf("Could not open %s\n", filename);
		return 0;
	}
	*filesize= GetFileSize(hFile, &eboot_size_upper);
	printf("%s size is %d bytes\n", filename, *filesize);
	if(eboot_size_upper) {
		printf("That file is bigger than 4 gb! Are you sure its a PSVita Physical Memdump? Not supported!\n");
		CloseHandle(hFile);
		return 0;
	}
	hMap = CreateFileMapping(hFile,NULL,PAGE_READONLY,0,0,NULL);
	if(!hMap) {
		printf("Could not create map.\n");
		CloseHandle(hFile);    
		return 0;
	}
	fileptr=MapViewOfFile(hMap,FILE_MAP_COPY ,0,0,0);
	if(!fileptr) {
		printf("Could not map view.\n");
		UnmapViewOfFile(hMap);
		CloseHandle(hFile);
		return 0;
	}	
	return fileptr;
}

void close_source_map(void) {
	
	UnmapViewOfFile(hMap);
	CloseHandle(hFile);
}
unsigned char * va2pa(int pa) {
	return  (g_ptr + (pa - g_base));
}
	


void write_page(unsigned int vaddr, unsigned int paddr, int size) {
	if ((vaddr >= g_start) && (vaddr < g_end)) {
		if (paddr == 0xFFFFFFFF) {
			memset(&g_outbuf[vaddr-g_start],0xFF,size);
		} else {
			memcpy(&g_outbuf[vaddr-g_start], va2pa(paddr),size);
		}
	}
}



	
void mmu_get_perms(int ap2, int ap1, int *ur, int *uw, int *pr, int *pw)
{
    if (0) // AFE disabled
    {
        *pw = (!ap2 && ap1);
        *pr = *pw || ap1;
        *ur = ap1 > 1;
        *uw = !ap2 && ap1 == 3;
    }
    else
    {
        *ur = ap1 > 1;
        *uw = !ap2 && ap1 > 1;
        *pr = 1;
        *pw = !ap2 && ap1 < 2;
    }
}

void mmu_dump_pages(unsigned int vaddr, unsigned int entry, unsigned int prev)
{
    int xn;
    int ng;
    int s;
    int ap2;
    int ap1;
    int pr;
    int pw;
    int ur;
    int uw;
    unsigned int paddr;

    if ((entry & 0x3) == 0x1) // large page
    {
        xn = entry & 0x8000;
        ng = entry & 0x800;
        s = entry & 0x400;
        ap2 = entry & 0x200;
        ap1 = (entry >> 4) & 3;
        mmu_get_perms(ap2, ap1, &ur, &uw, &pr, &pw);
        paddr = entry & 0xFFFF0000;
        if(g_print_only) printf("-[0x%08X] %s (0x%08X) PA:0x%08X NG:%u SH:%u UR:%u UW:%u PR:%u PW:%u XN:%u\n", vaddr, "Lg Page  ", entry, paddr, !!ng, !!s, !!ur, !!uw, !!pr, !!pw, !!xn);
        if(g_exfilename) write_page(vaddr,paddr,0x10000);
    }
    else if ((entry & 0x2)) // small page
    {
        xn = entry & 1;
        ng = entry & 0x800;
        s = entry & 0x400;
        ap2 = entry & 0x200;
        ap1 = (entry >> 4) & 3;
        mmu_get_perms(ap2, ap1, &ur, &uw, &pr, &pw);
        paddr = entry & 0xFFFFF000;
        if(g_print_only) printf("-[0x%08X] %s (0x%08X) PA:0x%08X NG:%u SH:%u UR:%u UW:%u PR:%u PW:%u XN:%u\n", vaddr, "Sm Page  ", entry, paddr, !!ng, !!s, !!ur, !!uw, !!pr, !!pw, !!xn);
        if(g_exfilename) write_page(vaddr,paddr,0x1000);
    }
    else
    {
        if(g_print_only) printf("-[0x%08X] %s\n", vaddr, "Unmapped ");
        if(g_exfilename) write_page(vaddr,0xFFFFFFFF,0x1000);
    }
}

void mmu_dump_sections(unsigned int vaddr, unsigned int entry, unsigned int prev)
{
    int ns;
    int ss;
    int ng;
    int s;
    int ap1;
    int ap2;
    int domain;
    int xn;
    int pr;
    int pw;
    int ur;
    int uw;
    unsigned int paddr;
    unsigned int i;
    unsigned int *tbl;
    unsigned int tblprev;
    

    if ((entry & 0x3) == 2) // section or supersection
    {
        ns = entry & 0x80000;
        ss = entry & 0x40000;
        ng = entry & 0x20000;
        s = entry & 0x10000;
        ap2 = entry & 0x8000;
        ap1 = (entry >> 10) & 3;
        domain = (entry >> 5) & 15;
        xn = entry & 0x10;
        mmu_get_perms(ap2, ap1, &ur, &uw, &pr, &pw);
        paddr = ss ? entry & 0xFF000000 : entry & 0xFFF00000;

        if(g_print_only) printf("[0x%08X] %s (0x%08X) PA:0x%08X NG:%u SH:%u UR:%u UW:%u PR:%u PW:%u XN:%u NS:%u DOM:%02X\n", vaddr, ss ? "S-Section " : "Section   ", entry, paddr, !!ng, !!s, !!ur, !!uw, !!pr, !!pw, !!xn, !!ns, domain);
        if(g_exfilename) write_page(vaddr,paddr,0x100000);
    }
    else if ((entry & 0x3) == 1) // page table
    {
        domain = (entry >> 5) & 15;
        ns = entry & 8;
        paddr = entry & 0xFFFFFC00;
        //printf("paddr = 0x%x\n", paddr);
        tbl =(int*) va2pa(paddr);
        if(g_print_only) printf("[0x%08X] %s (0x%08X) PA:0x%08X NS:%u DOM:%02X\n", vaddr, "Page TBL  ", entry, paddr, !!ns, domain);
        if (tbl)
        {
            tblprev = 0xffffffff;
            for (i = 0; i < 0x100; i++)
            {
                if((tbl[i] == 0) || (tbl[i] != tblprev)) mmu_dump_pages(vaddr+(i<<12), tbl[i], tblprev);
                tblprev = tbl[i];
            }
        }
        else
        {
            if(g_print_only) printf("!! WARNING CANNOT FIND VADDR FOR 0x%08X !!\n", paddr);
        }
    }
    else if ((entry & 0x3) == 0) // not mapped
    {
        if(g_print_only) printf("[0x%08X] %s\n", vaddr, "Unmapped  ");
    }
    else
    {
        if(g_print_only) printf("[0x%08X] %s\n", vaddr, "Invalid   ");
    }
}

void display_version(void) {
	printf("Vita Physical Memory Tool %d.%d - by Proxima (Build %s %s)\n", MAJVER, MINVER, __DATE__, __TIME__);
	printf("-----------------------------------------------------------------------\n\n");
}
void display_options(void) {
	printf("  Usage: vita-physmem-tool.exe [options]\n");
	printf("\noptions:\n");
	printf("  -i filename   [REQUIRED] specify the input physical dump file\n");
	printf("  -0 hex        [REQUIRED] physical file offset for TTBR0\n");
	printf("  -1 hex        [REQUIRED] physical file offset for TTBR1\n");
	printf("  -c dec        [REQUIRED] decimal value for TTBCR\n");
	printf("  -b hex        [REQUIRED] base address of physical device memory. (0x40300000)\n");
	printf("  -p            print dump of MMU tables\n");
	printf("  -d hex        start virtual address of memory you want to dump\n");
	printf("  -e hex        end virtual address of memory you want to dump\n");
	printf("  -f filename   output filename prefix for saving the memory range\n");
}

int find_arg(char *argv[], int argc, char *s) {
	int i;
	for(i=1;i<argc;i++) {
		if(strcmp(argv[i], s) == 0) return i;
	}
	return 0;
}

int process_args(char *argv[], int argc) {
	int pos;
	
	g_infilename=0;g_ttbr0=0;g_ttbr1=0;g_base=0;g_start=0;g_end=0;g_print_only=0;g_exfilename=0;
	
	if(argc < 3) return -1;

	if(find_arg(argv,argc,"-p")) g_print_only=1;

		
	pos=find_arg(argv,argc,"-i");
	if(((pos+1)<argc) && (pos>0)) {
		g_infilename=argv[pos+1];
	} else {
		g_infilename=0;
	}
	pos=find_arg(argv,argc,"-f");
	if(((pos+1)<argc) && (pos>0)) {
		g_exfilename=argv[pos+1];
	} else {
		g_exfilename=0;
	}
	pos=find_arg(argv,argc,"-0");
	if(((pos+1)<argc) && (pos>0)) {
		sscanf_s(argv[pos+1], "%x", &g_ttbr0);
	} else {
		g_ttbr0=0;
	}
	pos = find_arg(argv, argc, "-c");
	if (((pos + 1) < argc) && (pos > 0)) {
		sscanf_s(argv[pos + 1], "%d", &g_ttbcr);
	}
	else {
		g_ttbcr = 0;
	}
	pos=find_arg(argv,argc,"-1");
	if(((pos+1)<argc) && (pos>0)) {
		sscanf_s(argv[pos+1], "%x", &g_ttbr1);
	} else {
		g_ttbr1=0;
	}		
	pos=find_arg(argv,argc,"-b");
	if(((pos+1)<argc) && (pos>0)) {
		sscanf_s(argv[pos+1], "%x", &g_base);
	} else {
		g_base=0;
	}		
	pos=find_arg(argv,argc,"-d");
	if(((pos+1)<argc) && (pos>0)) {
		sscanf_s(argv[pos+1], "%x", &g_start);
	} else {
		g_start=0;
	}		
	pos=find_arg(argv,argc,"-e");
	if(((pos+1)<argc) && (pos>0)) {
		sscanf_s(argv[pos+1], "%x", &g_end);
	} else {
		g_end=0;
	}		
	if(!g_infilename) return -1;
	if(!g_ttbr0) return -1;
	if(!g_ttbr1) return -1;		
	if (!g_ttbcr) return -1;
	if(!g_base) return -1;

	return 0;
}

int main(int argc, char *argv[])
{
	int size;
    int ttbr0;
    int ttbr1;
    int ttbcr;
    int n;
    int i;
    
    unsigned int *vaddr[2];
    unsigned int entry;
    unsigned int prev;
		
	int res;

    display_version();
	if(process_args(argv,argc) != 0) {
		display_options();
		return -1;
	}

	g_ptr=load_file(g_infilename, &size);
	if(!g_ptr) {
		printf("File load of %s  did not work!\n", g_infilename);
		return -1;
	} else {
		printf("Loaded %s - 0x%x bytes\n", g_infilename,size);
	}
	if(g_exfilename) {
		g_outbuf=(unsigned char *) malloc(g_end-g_start);
		if(g_outbuf) memset(g_outbuf,0,(g_end-g_start));
		
		if(!g_outbuf) {
			printf("Can't allocate enough for storage of dump\n");
			return -1;
		}
		else {
			printf("Going to dump from 0x%08x to 0x%08x\n", g_start, g_end);
		}
	}
    ttbr0 = g_ttbr0;
    ttbr1 = g_ttbr1;
    ttbcr = g_ttbcr;
    printf("TTBR0: 0x%08X, TTBR1: 0x%08X, TTBCR: 0x%08X\n", ttbr0, ttbr1, ttbcr);

    n = ttbcr & 0x7;
    ttbr0 &= (unsigned int)((int)0x80000000 >> (31 - 14 + 1 - n));
    ttbr1 &= 0xFFFFC000;

    printf("N: 0x%02X\n", n);
    vaddr[0] =(int*) va2pa( ttbr0);
    vaddr[1] =(int*) va2pa( ttbr1);
    printf("TBBR0 (physical): 0x%08X\n", ttbr0 );
    printf("TBBR1 (physical): 0x%08X\n", ttbr1 );
    if (vaddr[0] == NULL || vaddr[1] == NULL)
    {
        printf("Invalid addr.\n");
        return 0;
    }
    //printf("vaddr[0] = 0x%x\n", vaddr[0]);
    //printf("vaddr[1] = 0x%x\n", vaddr[1]);
	if(g_print_only) {
		printf("Dumping TTBR0...\n");
		prev = 0xffffffff;
		for (i = 0; i < ((1 << 12) - n); i++)
		{
			entry = vaddr[0][i];
			// printf("entry = 0x%x\n", entry);
			mmu_dump_sections(i<<20, entry, prev);
			prev = entry;
		}
	
		if (n)
		{
			printf("Dumping TTBR1...\n");
			prev = 0xffffffff;
			for (i = ((~0xEFFF & 0xFFFF) >> n); i < 0x1000; i++)
			{
				entry = vaddr[1][i];
				mmu_dump_sections(i<<20, entry, prev);
				prev = entry;
			}
   		}	
  	}
  	if(g_exfilename) {
  		char fname[0x100];
  		memset(fname,0,0x100);
  		sprintf_s(fname,0x100,"%s-0x%x-0x%x.bin", g_exfilename, g_start, g_end);
  		res=write_file(fname, g_outbuf, g_end-g_start);
  		if(res!= ( g_end-g_start)) {
  			printf("Error writing complete file %s\n", fname);
  		} else {
  			printf("File %s written successfully\n", fname);
  		}
  	}
  	if(g_outbuf) free (g_outbuf);
		close_source_map();
    return 0;
}
