/* GPLv2 (c) Airbus */
#include <debug.h>
#include <segmem.h>

/*
L'objectif ici est de réussir à passer du mode noyau (Ring 0) au mode utilisateur (Ring 3) en trompant le processeur 
à l'aide de l'instruction IRET (Interrupt Return).

Normalement, IRET sert à revenir d'une interruption. 
Mais comme le processeur s'attend exactement à retrouver sur la pile un contexte d'interruption matériel 
(SS, ESP, EFLAGS, CS, EIP), nous allons simuler manuellement ce contexte sur la pile avec des push, 
puis appeler IRET. Le CPU va croire qu'il revient d'une interruption, ce qui lui fait dépiler ces registres 
et basculer, par la même occasion, le niveau de privilège (CPL) de 0 à 3.
*/

void userland() {
   asm volatile ("mov %eax, %cr0");
}

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
   // Question 1
    // chargement de la nouvelle GDT
    gdt_reg_t my_gdtr;
    my_gdtr.limit = sizeof(my_new_gdt) - 1; 
    my_gdtr.desc  = my_new_gdt; 
    set_gdtr(my_gdtr);

    // maj des segments de données en ring 0 (indice 2)
    uint16_t data_selector = 0x10; // (2 << 3) | 0
    set_ds(data_selector);
    set_es(data_selector);
    set_fs(data_selector);
    set_gs(data_selector);

    // maj du segment de code en ring 0 (indice 1) via un saut lointain
    set_cs(0x08); // (1 << 3) | 0

    debug("la GDT est chargée, on passe en mode utilisateur (ring 3) avec IRET...\n");

   // Question 2

    // on alloue une pile dédiée au mode utilisateur
    static uint32_t user_stack[1024];
    uint32_t user_esp = (uint32_t)&user_stack[1024]; // on recup le sommet de la pile

    // simulation du contexte d'interruption et saut avec IRET
    // indices de la GDT : 
    //   indice 4 (Code ring 3) : (4 << 3) | 3 = 0x23
    //   indice 5 (Data/Stack ring 3) : (5 << 3) | 3 = 0x2b
    asm volatile (
        "push $0x2b\n\t"           // SS (Stack Segment ring 3)
        "push %0\n\t"              // ESP
        "pushf\n\t"                // EFLAGS
        "push $0x23\n\t"           // CS (Code Segment ring 3)
        "push %1\n\t"              // EIP (adresse de userland)
        "iret\n\t"                  // Bascule en Ring 3 et exécute userland()
        : 
        : "r" (user_esp), "r" (userland)
        : "memory"
    );
}
/* Ca fonctionne :
Il y a bien une erreur.

output :

la GDT est chargée, on passe en mode utilisateur (ring 3) avec IRET...
qemu: fatal: invalid tss type
EAX=00306560 EBX=0002be40 ECX=00304960 EDX=00303f83
ESI=0002bfc2 EDI=0002bfc3 EBP=0030655c ESP=0030655c
EIP=00303f86 EFL=00000002 [-------] CPL=3 II=0 A20=1 SMM=0 HLT=0
ES =0000 00000000 ffffffff 00cf1300
CS =0023 00000000 ffffffff 00cffa00 DPL=3 CS32 [-R-]
SS =002b 00000000 ffffffff 00cff200 DPL=3 DS   [-W-]
DS =0000 00000000 ffffffff 00cf1300
FS =0000 00000000 ffffffff 00cf1300
GS =0000 00000000 ffffffff 00cf1300
LDT=0000 00000000 0000ffff 00008200 DPL=0 LDT
TR =0000 00000000 0000ffff 00008b00 DPL=0 TSS32-busy
GDT=     00304910 0000002f
IDT=     00304d60 000007ff
CR0=00000011 CR2=00000000 CR3=00000000 CR4=00000000
DR0=00000000 DR1=00000000 DR2=00000000 DR3=00000000 
DR6=ffff0ff0 DR7=00000400
CCS=00000000 CCD=00301fd0 CCO=EFLAGS
EFER=0000000000000000
FCW=037f FSW=0000 [ST=0] FTW=00 MXCSR=00001f80
FPR0=0000000000000000 0000 FPR1=0000000000000000 0000
FPR2=0000000000000000 0000 FPR3=0000000000000000 0000
FPR4=0000000000000000 0000 FPR5=0000000000000000 0000
FPR6=0000000000000000 0000 FPR7=0000000000000000 0000
XMM00=0000000000000000 0000000000000000 XMM01=0000000000000000 0000000000000000
XMM02=0000000000000000 0000000000000000 XMM03=0000000000000000 0000000000000000
XMM04=0000000000000000 0000000000000000 XMM05=0000000000000000 0000000000000000
XMM06=0000000000000000 0000000000000000 XMM07=0000000000000000 0000000000000000

*/