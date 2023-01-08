#include <stdio.h>
#include <sel4/sel4.h>
#include <sel4platsupport/bootinfo.h>
#include <stdint.h>

const seL4_BootInfo *info;

struct Nodo{
    seL4_Word hasiera;
    seL4_Word bukaera;
    seL4_Word hurrengoa;
};

struct Nodo* free_list = NULL;
struct Nodo* malloc_list = NULL;
int alignment = 0;

const seL4_Word tam = sizeof(struct Nodo);


/*
* Funtzio honek, helbide bat jasota, memoriari dagokion alignmentera egokitzen du. Free egiten ari bagara, helbidea txikiagotu egingo da
* Malloc egiten ari bagara, helbidea handiagotu egingo da.
* @param Memoria helbide bat eta free edo malloc den adierazten duen zenbakia.
* @return Helbide konpondua
**/
seL4_Word alignment_konponketa(seL4_Word addr, int free){

	if (free){
		return addr - (alignment - addr%alignment);		
	}
	if (addr%alignment!=0){
		return addr + (alignment - addr%alignment);
	}
	return addr;
}

/*
* Funtzio honek jasotako zenbakiari ^2 eragiketa aplikatzen dio.
* @param Tratatuko den zenbakia.
* @return Sarrerako zenbakia^2.
**/
int pow2(int size){
	int biderk = 1;
	for (int i = 0; i < size; i++){
		biderk = biderk*2;
	}
	return biderk;
}

/*
* Funtzio honek bi eskualde kontsekutiboak diren detektatzen du.
* @param Bi eskualdeen bootinfoko indizeak.
* @return true kontsekutiboak badira. Bestela false.
**/
int kontsekutiboak(int i, int j){

    int add1 = info->untypedList[i].paddr;
    int add2 = info->untypedList[j].paddr;

    int size = info->untypedList[j].sizeBits;

    int sizea = pow2(size);
    return (add1+sizea)==add2; 
}


/*
* Funtzio honek, memoriako hutsuneak mapeatzeko erabiliko den nodo baten helbidea itzultzen du
* @return Memoriako hutsunea kudeatzeko erabiliko den nodoa
**/
static struct Nodo* FreeNodoAlloc(){
	static struct Nodo nodo_list[100];
	static int i=0;
	i++;
	return &nodo_list[i-1];

}

/*
* Funtzio honek, memoriako erreserbak mapeatzeko erabiliko den nodo baten helbidea itzultzen du
* @return Memoriako erreserbak kudeatzeko erabiliko den nodoa
**/
static struct Nodo* MallocNodoAlloc(){
	static struct Nodo nodo_list[1000];
	static int i=0;
	i++;
	return &nodo_list[i-1];

}

/*
* Funtzio honek, sarrerako memoria hasiera eta amaiera dituen Nodo bat sortu eta hutsuneen zerrendan sartuko du
* @param Memoria eskualdearen hasiera eta amaiera
**/
static void free_nodo_berri(seL4_Word has, seL4_Word buk){
	static struct Nodo* oraingo_Nodoa = NULL;
	static struct Nodo* aurreko_Nodoa = NULL;
	
	oraingo_Nodoa = FreeNodoAlloc();
	
	oraingo_Nodoa->hasiera = has;
	oraingo_Nodoa->bukaera = buk;
	
	if(free_list==NULL){
		free_list = oraingo_Nodoa;
	}
	if(aurreko_Nodoa!=NULL)
		aurreko_Nodoa->hurrengoa = oraingo_Nodoa;
	aurreko_Nodoa = oraingo_Nodoa;
}

/*
* Funtzio honek, sarrerako memoria hasiera eta amaiera dituen Nodo bat sortu eta erreserbatuen zerrendan sartuko du
* @param Memoria eskualdearen hasiera eta amaiera
**/
static void malloc_nodo_berri(seL4_Word has, seL4_Word buk){
	static struct Nodo* oraingo_Nodoa = NULL;
	static struct Nodo* aurreko_Nodoa = NULL;
	oraingo_Nodoa = MallocNodoAlloc();
	oraingo_Nodoa->hasiera = has;
	oraingo_Nodoa->bukaera = buk;
	
	if(malloc_list==NULL){
		malloc_list = oraingo_Nodoa;
	}
	if(aurreko_Nodoa!=NULL)
		aurreko_Nodoa->hurrengoa = oraingo_Nodoa;
	aurreko_Nodoa = oraingo_Nodoa;
}

/*
* Funtzio honek, memoria sistema hasieratuko du alignmenta kontuan hartuta, erabilgarri dauden memoria eskualdeak hutsuneen zerrendan gehituz
* @param Memoria sistemak izango duen alignmenta
**/
static void init_memory_system(int alignment_value) {

	int i;
	alignment = alignment_value;

	struct Nodo* oraingo_Nodoa = NULL;
	struct Nodo* aurreko_Nodoa = NULL;

	int nodo_kop = 0;
	int lag = 0;
	
	seL4_Word hasiera = NULL;
	seL4_Word bukaera = NULL;

	for (i = 0; i < info->untyped.end-info->untyped.start - 1; i++) {
		if (nodo_kop>4){
			break;
		}
	      if (!(info->untypedList[i].isDevice)){
		
			if (hasiera==NULL){
				hasiera = info->untypedList[i].paddr;
			}
			
			if(!kontsekutiboak(i,i+1)){
			    bukaera = info->untypedList[i].paddr+pow2(info->untypedList[i].sizeBits);
			    bukaera = alignment_konponketa(bukaera, 1);
			    free_nodo_berri(hasiera, bukaera);
			    hasiera = NULL;
			    nodo_kop++;
			}
		
	      }
		

	}
}

/*
* Funtzio honek, adierazitako memoria kopurua erreserbatuko du eta erreserbatutako eskualdearen hasierako helbidea itzuliko du
* @param Erreserbatu nahi den memoria kopurua
* @return Erreserbatutako eskualdearen hasierako helbidea
**/
static seL4_Word allocate(int sizeB){
	int sizea = pow2(sizeB);
	struct Nodo* it = free_list;
	seL4_Word erantzuna;
	sizea = alignment_konponketa(sizea, 0);
	while (it!=NULL){
		if((it->bukaera - it->hasiera) >= sizea){
			malloc_nodo_berri(it->hasiera, it->hasiera+sizea);
			erantzuna = it->hasiera;
			it->hasiera = it->hasiera+sizea;
			break;
		}
		it = it->hurrengoa;
	}
	if (it==NULL){
		printf("Ezin izan da memoria erreserbatu\n");
		return -1;
	}
	return erantzuna;
}

/*
* Funtzio honek, memoria eskualde erreserbatu baten hasiera emanik, eskualde hori askatu eta hutsuneen zerrendan sartuko du.
* @param Erreserbatutako memoria eskualdearen hasierako helbidea
**/
static void release(seL4_Word addr){
	static int t_kont = 0;
	struct Nodo* it = malloc_list;
	struct Nodo* askatu;
	struct Nodo* aurreko_it = NULL;
	while (it!=NULL){
		if(addr==it->hasiera){
			askatu=it;
			break;
		}
		aurreko_it = it;
		it = it->hurrengoa;
	}
	if (it==NULL){
		printf("Helbide hori ez da erreserbatua izan\n");
		return ;
	}
	if (aurreko_it!=NULL){
		aurreko_it->hurrengoa = it->hurrengoa;
	}else{
		malloc_list = it->hurrengoa;
	}
	aurreko_it = free_list;
	it = aurreko_it->hurrengoa;
	while (it!=NULL){
		if (it->hasiera > addr){
			break;
		}
		aurreko_it = it;
		it = it->hurrengoa;
		
	}
	if (addr<aurreko_it->hasiera){
		free_list = askatu;
		askatu->hurrengoa = aurreko_it;
	}else{
		aurreko_it->hurrengoa = askatu;
		askatu->hurrengoa = it;
	}
	t_kont++;
	if (t_kont==10){
		trinkotzea();
		t_kont = 0;
	}
	
}

/*
* Funtzio honek, hutsuneen zerrenda iteratu eta hutsune kontsekutiboak baleude, hutsuneak elkartzen ditu konputazio kostua arintzearren.
**/
void trinkotzea(){
	struct Nodo* it = free_list;
	struct Nodo* aurreko_it = NULL;
	while (it!=NULL){
		if(aurreko_it!=NULL && aurreko_it->bukaera==it->hasiera){
			aurreko_it->bukaera = it->bukaera;
			aurreko_it->hurrengoa = it->hurrengoa;
			it = it->hurrengoa;
			continue;
		}
		aurreko_it = it;
		it = it->hurrengoa;
	}

}


int main(void) {
    info = platsupport_get_bootinfo();
    init_memory_system(8);
    printf(">>>\nExekuzioa amaitu da\n>>>\n");
    return(0);
}
