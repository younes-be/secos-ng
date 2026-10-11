/* GPLv2 (c) Airbus */
#include <debug.h>
#include <intr.h>
 
// tout l'asm doit etre dans le même inline sinon le compilateur désaligne ce qu'il ya dans la piles.
// L'attribut "naked" est indispensable ici : il empêche le compilateur C 
// d'ajouter automatiquement un prologue et un épilogue (gestion de pile/EBP). 
// Sans lui, le code généré perturberait la pile et provoque 
// une exception General Protection (#GP) lors de l'exécution de l'instruction iret.
void __attribute__((naked)) bp_handler() { 
    asm volatile (
        "pusha\n\t"                // Sauvegarde des registres généraux (Q8)
        
        // 2. Appel de la logique / affichage
        // On peut appeler debug() ici car les registres sont protégés par pusha
        "call bp_handler_body\n" 
        
        "popa\n\t"                 // 3. Restauration des registres
        "iret\n\t"                 // 4. Retour matériel (Q9)
    );
}

void bp_handler_body() {
	uint32_t val;
   	asm volatile ("mov 4(%%ebp), %0":"=r"(val));
	debug("Valeur à ebp+4 (adresse de retour) : 0x%x\n", val); // 0x303fb9
	// réponse question 7
	/*
	00303fb5 <bp_trigger>:
	303fb5:	55                   	push   %ebp
	303fb6:	89 e5                	mov    %esp,%ebp
	303fb8:	cc                   	int3
	303fb9:	90                   	nop
	303fba:	5d                   	pop    %ebp
	303fbb:	c3                   	ret

  	Cette valeur est l'adresse de retour après le int3.
	*/

	//Question 8
	/*
	Qu'est-ce qui n'est pas stocké par le CPU à l'arrivée d'une interruption et 
	qu'il est impératif de sauvegarder avant tout traitement de l'interruption ?

	Quand une interruption arrive, le processeur empile automatiquement certains registres, 
	mais il ne sauvegarde pas les registres généraux (comme EAX, EBX, ECX, EDX, ESI, EDI, etc.). 
	Si votre fonction C modifie ces registres, elle va écraser le travail du programme d'origine. 
	Il faut donc les sauvegarder au tout début avec pusha (qui empile tous les registres généraux d'un coup).
	*/

	debug("Breakpoint (#BP) intercepté !\n");

	//Question 9
	/*
	Par quelle instruction doit se terminer la routine pour que le noyau rende la main à la fonction tp() ?

	pour quitter une interruption et rendre la main proprement, un simple ret ne suffit pas : 
	il faut utiliser iret (Interrupt Return), qui va dépiler tout le contexte matériel que 
	le CPU avait mis en place (EFLAGS, CS, EIP, etc.).
	*/
}

void bp_trigger() {
	// pour déclencher l'exception #BP (Breakpoint)
    asm volatile ("int3");

	// Question 10 : print debug pour check que c'est bien géré.
	debug("Après int3, tout va bien !\n");
}

void tp() {
	// TODO print idtr
	idt_reg_t idtr;
    get_idtr(idtr);
    debug("Adresse de l'idt : %p\n", (void *)idtr.addr);

	// on modifie l'entrée numéro 3 (#BP) pour pointer vers bp_handler
	build_int_desc(&idtr.desc[3], get_cs(), (uint32_t)bp_handler);
	// TODO call bp_trigger
   	bp_trigger();

	// réponse question 5
	/*
	Quelle est la dernière instruction de cette fonction ?
	C'est l'instruction ret (Return).

	Quel est son impact sur la pile ?
	L'instruction ret dépile uniquement l'adresse de retour (le pointeur d'instruction EIP) qui avait été mise là par un simple appel de fonction classique (call).

	Est-ce cohérent avec ce qui était sur la pile au moment de l'arrivée d'une interruption ?
	Non.
	Lorsqu'une vraie interruption ou exception survient, le processeur empile un contexte matériel complet 
	(au minimum EFLAGS, CS, EIP, et parfois un code d'erreur). 
	Si la fonction se termine par un simple ret, le processeur ne nettoie pas ce surplus 
	et l'adresse de retour attendue par ret ne correspond plus à rien de valide, 
	ce qui corrompt la pile et provoque un crash (d'où l'exception #6 Invalid Opcode observée juste avant).
	*/

	

}

/* CONCLUSION Question 11:

Incompatibilité structurelle du C pur : 
Un gestionnaire d'interruption écrit entièrement en C standard échoue car le compilateur génère automatiquement un prologue et un épilogue 
de fonction (push %ebp, mov %esp, %ebp, etc.) 
ainsi qu'une instruction de fin ret. Ces mécanismes modifient le pointeur de pile (esp) et le cadre de pile de manière incompatible 
avec ce que le processeur attend lors d'une interruption.

Corruption des registres généraux : 
Le code C (et les appels de fonctions qu'il contient) modifie les registres du processeur (EAX, EBX, etc.). 
Sans une sauvegarde explicite et globale (pusha), l'état du programme interrompu est irrémédiablement écrasé.

Nécessité de l'assembleur (ou de l'hybride) : 
L'assembleur est indispensable pour les gestionnaires d'interruptions car il offre un contrôle total et de bas niveau sur :

    La préservation immédiate du contexte matériel et logiciel (pusha / popa).

    L'interfaçage direct avec la pile matérielle générée par le CPU.

    L'utilisation obligatoire de l'instruction de retour spécifique iret (au lieu de ret) pour restaurer correctement les registres d'état, 
	le segment de code (CS) et le pointeur d'instruction (EIP).


*/