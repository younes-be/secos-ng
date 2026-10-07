/* GPLv2 (c) Airbus */
#include <debug.h>
#include <segmem.h>
#include <string.h>

#define TP1_FATAL_NONE          0
#define TP1_FATAL_Q8_CS         1
#define TP1_FATAL_Q11_LIMIT     2
#define TP1_FATAL_Q13_SS        3
#define TP1_FATAL_Q13_FARJUMP   4

// Choisir un seul test fatal à la fois. Les autres tests restent exécutables.
#ifndef TP1_FATAL_TEST
#define TP1_FATAL_TEST TP1_FATAL_NONE
#endif

void userland() {
   asm volatile ("mov %eax, %cr0");
}

void print_gdt_content(gdt_reg_t gdtr_ptr) {
    seg_desc_t* gdt_ptr;
    gdt_ptr = (seg_desc_t*)(gdtr_ptr.addr);
    int i=0;
    while ((uint32_t)gdt_ptr < ((gdtr_ptr.addr) + gdtr_ptr.limit)) {
        uint32_t start = gdt_ptr->base_3<<24 | gdt_ptr->base_2<<16 | gdt_ptr->base_1;
        uint32_t end;
        if (gdt_ptr->g) {
            end = start + ( (gdt_ptr->limit_2<<16 | gdt_ptr->limit_1) <<12) + 4095;
        } else {
            end = start + (gdt_ptr->limit_2<<16 | gdt_ptr->limit_1);
        }
        debug("%d ", i);
        debug("[0x%x ", start);
        debug("- 0x%x] ", end);
        debug("seg_t: 0x%x ", gdt_ptr->type);
        debug("desc_t: %d ", gdt_ptr->s);
        debug("priv: %d ", gdt_ptr->dpl);
        debug("present: %d ", gdt_ptr->p);
        debug("avl: %d ", gdt_ptr->avl);
        debug("longmode: %d ", gdt_ptr->l);
        debug("default: %d ", gdt_ptr->d);
        debug("gran: %d ", gdt_ptr->g);
        debug("\n");
        gdt_ptr++;
        i++;
    }
}


// Questions 5, 9 et 12 : GDT alignée sur 8 octets.
__attribute__((aligned(8))) seg_desc_t my_new_gdt[6] = {
    // indice 0 : Entrée NULL
    { .p = 0 },

    // indice 1 : Code, 32 bits RX, Flat, Ring 0
    { .base_1 = 0, .base_2 = 0, .base_3 = 0, .limit_1 = 0xFFFF, .limit_2 = 0xF,
      .type = SEG_DESC_CODE_XR, .s = 1, .dpl = SEG_SEL_KRN, .p = 1, .d = 1, .g = 1 },

    // indice 2 : Données, 32 bits RW, Flat, Ring 0
    { .base_1 = 0, .base_2 = 0, .base_3 = 0, .limit_1 = 0xFFFF, .limit_2 = 0xF,
      .type = SEG_DESC_DATA_RW, .s = 1, .dpl = SEG_SEL_KRN, .p = 1, .d = 1, .g = 1 },

    // indice 3 : Q9, data ring 0, base 0x600000, limite inclusive 31 (32 octets)
    { .base_1 = 0, .base_2 = 0x60, .base_3 = 0, .limit_1 = 31, .limit_2 = 0,
      .type = SEG_DESC_DATA_RW, .s = 1, .dpl = SEG_SEL_KRN, .p = 1, .d = 1, .g = 0 },

    // indice 4 : Code, 32 bits RX, flat, ring 3
    { .base_1 = 0, .base_2 = 0, .base_3 = 0, .limit_1 = 0xFFFF, .limit_2 = 0xF,
      .type = SEG_DESC_CODE_XR, .s = 1, .dpl = SEG_SEL_USR, .p = 1, .d = 1, .g = 1 },

    // indice 5 : Données, 32 bits RW, flat, ring 3
    { .base_1 = 0, .base_2 = 0, .base_3 = 0, .limit_1 = 0xFFFF, .limit_2 = 0xF,
      .type = SEG_DESC_DATA_RW, .s = 1, .dpl = SEG_SEL_USR, .p = 1, .d = 1, .g = 1 }
};


void tp() {
	gdt_reg_t gdtr_p;
	get_gdtr(gdtr_p);
	// Question 2 : Affichage du contenu de table de type GDT
	debug("GDTR : limit 0x%x , base : 0x%lx\n", gdtr_p.limit, gdtr_p.addr); 
	print_gdt_content(gdtr_p) ;

	// Question 3 :
	// On pourrait aussi utiliser get_ss() etc .. mais il manque cs dans le .h
	uint16_t cs,ss, ds, es, fs, gs ;
	cs = get_seg_sel(cs) >> 3; 
	ss = get_seg_sel(ss) >> 3;
	ds = get_seg_sel(ds) >> 3;
	es = get_seg_sel(es) >> 3;
	fs = get_seg_sel(fs) >> 3;
	gs = get_seg_sel(gs) >> 3;

	// Affichage
    debug("CS (Code Segment)  Index: %u \n", cs);
    debug("SS (Stack Segment) Index: %u \n", ss);
    debug("DS (Data Segment)  Index: %u \n", ds);
    debug("ES (Extra Segment) Index: %u \n", es);
    debug("FS (Segment F)     Index: %u \n", fs);
    debug("GS (Segment G)     Index: %u \n", gs);
    debug("--------------------------------------\n");

/* Résultats attendus Q2-Q4 (les sélecteurs affichés sont les index) :
CS (Code Segment)  Index: 1 
SS (Stack Segment) Index: 2 
DS (Data Segment)  Index: 2 
ES (Extra Segment) Index: 2 
FS (Segment F)     Index: 2 
GS (Segment G)     Index: 2 

Q1 : SGDT copie en mémoire le pseudo-descripteur GDTR (limite 16 bits puis base)
à l'adresse de son opérande mémoire. La limite est la taille de la GDT moins 1.

Q4 : CS pointe sur le descripteur de code, les autres registres sur le
descripteur de données. Les segments ont des bases nulles et des limites
maximales : la segmentation ne sépare donc pas les zones mémoire. Les types
code/data distinguent les droits d'accès, mais ne constituent pas à eux seuls
une isolation mémoire du noyau. En particulier, avec les deux segments en
ring 0, il n'y a pas de séparation de privilèges entre noyau et utilisateur.
*/



// Question 6 : chargement de la nouvelle GDT dans le GDTR et mise à jour des registres
    gdt_reg_t my_gdtr;
    // La limite est égale à la taille - 1
    my_gdtr.limit = sizeof(my_new_gdt) - 1; 
    my_gdtr.desc  = my_new_gdt; // adresse de base du tableau gdt

    // charge la nouvelle table dans le processeur
    set_gdtr(my_gdtr);

    // on génère les sélecteurs bruts grâce aux macros fournies dans segmem.h
    // Index 1 = Code, Index 2 = Données/Pile (RPL = Kernel)
    //const uint16_t code_selector = gdt_krn_seg_sel(1); // vaut 0x08 ((1<<3) | 0)
    uint16_t data_selector = 0x10 ;// vaut 0x10 ((2<<3) | 0)

    // on met à jour les registres de segments de données et pile
    set_ds(data_selector);
    set_ss(data_selector);
    set_es(data_selector);
    set_fs(data_selector);
    set_gs(data_selector);

    // et on met à jour le registre de code CS via un saut lointain
    set_cs(0x08); 
	// On utilise pas de variable car une erreur provient de la macro set_cs définie dans segmem.h. La contrainte "i"(_cs) exige que la valeur passée soit une constante connue à la compilation .

    // Question 7 : On check les valeurs de la GDT
    debug("\nContenu de la NOUVELLE GDT\n");
    gdt_reg_t current_gdtr;
    get_gdtr(current_gdtr);
    print_gdt_content(current_gdtr);


    // Q8.1 : un segment de code lisible peut être chargé dans DS.
    debug("Q8.1 : chargement du segment de code dans DS\n");
    uint16_t code_selector = get_seg_sel(cs);
    set_ds(code_selector);
    set_ds(0x10);
    debug("Q8.1 : selecteur CS (0x%x) charge dans DS avec succes\n",
          code_selector);

    // Q8.2 : à tester séparément (set_cs effectue un saut lointain).
    // Le processeur doit déclencher #GP : CS ne peut pas référencer un segment
    // de données. Sélectionner TP1_FATAL_Q8_CS pour observer l'exception.
#if TP1_FATAL_TEST == TP1_FATAL_Q8_CS
    debug("Q8.2 : #GP attendue lors du chargement data dans CS\n");
    set_cs(0x10);
#endif

    /*
     * Q8 : le segment de code de l'index 1 est lisible, donc son chargement
     * dans DS réussit. Cela ne permet pas d'écrire via ce descripteur. Le
     * segment de données de l'index 2 dans CS déclenche #GP (sélecteur fautif
     * 0x10), car CS doit désigner un segment de code exécutable.
     */

    // Q10 : ES:0 utilise le descripteur d'index 3 (base 0x600000, limite 31).
    char src[64];
    char *dst = (char *)0;
    memset(src, 0xff, sizeof(src));
    set_es(0x18);
    _memcpy8(dst, src, 32);
    debug("Q10 : copie de 32 octets reussie via ES:0\n");

    /*
     * Q10 écrit aux adresses linéaires 0x600000..0x60001f, car la destination
     * effective est ES.base + EDI. En l'absence de pagination, 
     * ces adresses correspondent à la mémoire physique.
     *
     * Q11 : choisir TP1_FATAL_Q11_LIMIT ci-dessus pour exécuter le test.
     * REP MOVSB copie les 32 premiers octets, puis déclenche #GP au 33e
     * (limite inclusive 31). L'exception est fatale dans SECOS.
     */
#if TP1_FATAL_TEST == TP1_FATAL_Q11_LIMIT
    set_es(0x18);
    _memcpy8(dst, src, 64);
    debug("Q11 : aucun #GP ; emulateur/configuration ne respecte pas la limite du segment\n");
#endif

    // Q13 : les sélecteurs de données ring 3 se chargent depuis CPL 0 lorsque
    // RPL=3 et DPL=3, car max(CPL,RPL) <= DPL.
    set_ds(0x2b);
    set_es(0x2b);
    set_fs(0x2b);
    set_gs(0x2b);
    debug("Q13 : DS/ES/FS/GS charges avec le selecteur ring 3\n");

    /*
     * Q12 : index 4 = code ring 3 (0x23), index 5 = data ring 3 (0x2b),
     * tous deux flat et en 32 bits.
     *
     * Q13 : choisir TP1_FATAL_Q13_SS ou TP1_FATAL_Q13_FARJUMP ci-dessus pour
     * tester séparément les deux transferts, qui provoquent chacun #GP.
     * SS exige CPL = DPL = RPL, mais le CPL vaut encore 0. Un JMP direct ne
     * permet pas non plus de passer à un niveau moins privilégié.
     * Pour passer en ring 3, le noyau doit passer par un call gate.
     */
#if TP1_FATAL_TEST == TP1_FATAL_Q13_SS
    debug("Q13 : #GP attendue lors du chargement ring 3 dans SS\n");
    set_ss(0x2b);
#elif TP1_FATAL_TEST == TP1_FATAL_Q13_FARJUMP
    fptr32_t userland_ptr = {
        .offset = (uint32_t)userland,
        .segment = 0x23
    };
    debug("Q13 : #GP attendue lors du far jump direct vers ring 3\n");
    farjump(userland_ptr);
#endif	
	
}
