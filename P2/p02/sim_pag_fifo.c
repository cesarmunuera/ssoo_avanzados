/*
    sim_pag_fifo.c
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sim_paginacion.h"

// Función que inicia las tablas

void iniciar_tablas (ssistema * S)
{
    int i;

    // Páginas a cero
    memset (S->tdp, 0, sizeof(spagina)*S->numpags);

    // Pila LRU vacía
    S->lru = -1;

    // Tiempo LRU(t) a cero
    S->reloj = 0;

    // Lista circular de marcos libres
    for (i=0; i<S->nummarcos-1; i++)
    {
        S->tdm[i].pagina = -1;
        S->tdm[i].sig = i+1;
    }

    S->tdm[i].pagina = -1;  // Ahora i == nummarcos-1
    S->tdm[i].sig = 0;      // Cerrar lista circular
    S->listalibres = i;     // Apuntar al último

    // Lista circular de marcos ocupados vacía
    S->listaocupados = -1;
}

// Funciones que simulan el hardware de la MMU

unsigned sim_mmu (ssistema * S, unsigned dir_virtual, char op)
{
    unsigned dir_fisica;
    int pagina, marco, desplazamiento;

    pagina = dir_virtual / S-> tampag ; 		// Cociente
    desplazamiento = dir_virtual % S-> tampag ; 	// Resto


    if (pagina<0 || pagina>=S-> numpags){
		S-> numrefsilegales ++;
		return ~0U ; 		// Devolver dir . física FF ... F
    }
	

    // Si la pagina en cuestion no se encuentra cargada en memoria fisica, 		provoca un trap. Tratar_fallo_pagina es el que emula la rutina del 			SSOO para resolver este problema.
    if (!S->tdp[pagina].presente) // No presente :
		tratar_fallo_de_pagina(S,dir_virtual); // FALLO DE PÁG .

	// Ahora ya está presente, el SSOO ha cargado la página en el marco, 		y ha modificado la tabla de páginas.
	marco = S->tdp[pagina].marco ; 					// Calcular dirección
	dir_fisica = marco * S->tampag + desplazamiento;// física

	referenciar_pagina (S, pagina, op );

	// Volcamos la informacion del acceso a memoria de forma detallada, si 		lo solicita el usuario.
	if (S-> detallado){
		printf("\t%c %u==P%d(M%d)+%d\n", op, dir_virtual, pagina, marco, 			desplazamiento);
	}

    return dir_fisica;
}

void referenciar_pagina (ssistema * S, int pagina, char op)
{
    if (op=='L')                         // Si es una lectura,
        S->numrefslectura ++;            // contarla
    else if (op=='E')
    {                                    // Si es una escritura,
        S->tdp[pagina].modificada = 1;   // contarla y marcar la
        S->numrefsescritura ++;          // página como modificada
    }
}



// Funciones que simulan el sistema operativo

void tratar_fallo_de_pagina (ssistema * S, unsigned dir_virtual)
{
    int pagina, victima, marco, ult;

	// Debemos calcular el numero de pagina que provoco el fallo. De paso, 		incrementamos el contador de fallos de pagina, y mostramos un mensaje 		por pantalla si el simulador está en modo 'D' (detallado).
	S-> numfallospag ++;
	pagina = dir_virtual / S->tampag;
	if (S-> detallado){
		printf ("@ ¡FALLO DE PAGINA en P%d!\n", pagina);
	}

	// En caso de que haya marcos libres, bastara con sacar un marco de la 		lista de marcos vacios. Lo ocupamos con la página solicitada.
	if (S->listalibres !=-1) 					// Si hay marcos libres:
	{
		ult = S->listalibres; 					// Último de la lista
		marco = S->tdm[ult].sig; 				// Tomar el sig.(el 1º)
		if (marco == ult) 						// Si son el mismo, es que
			S->listalibres =-1; 				// sólo quedaba uno libre
		else
			S->tdm[ult].sig = S->tdm[marco].sig; // Si no, lo coge y 													//actualiza el segundo 
												//	en el primero

		ocupar_marco_libre (S, marco, pagina);
	}else 										// Si no hay marcos libres:
	{
		victima = elegir_pagina_para_reemplazo ( S );
		reemplazar_pagina (S , victima , pagina );
	}


}


int elegir_pagina_para_reemplazo (ssistema * S)
{
    int marco, victima;

	// Cogemos el primer marco (sig del ultimo)
    marco = S-> tdm[S-> listaocupados].sig;
    victima = S->tdm[marco].pagina;

	// Apuntamos listaocupados al primero, para ir rotando por los marcos 		(ES FIFO). Es como mover el *
	S-> listaocupados = marco; 

    if (S->detallado)
        printf ("@ Eligiendo (FIFO) P%d de M%d para "
                "reemplazarla\n", victima, marco);

    return victima;
}

void reemplazar_pagina (ssistema * S, int victima, int nueva)
{
    int marco;

    marco = S->tdp[victima].marco;

    if (S->tdp[victima].modificada)
    {
        if (S->detallado)
            printf ("@ Volcando P%d modificada a disco para "
                    "reemplazarla\n", victima);

        S->numescrpag ++;
    }

    if (S->detallado)
        printf ("@ Reemplazando víctima P%d por P%d en M%d\n",
                victima, nueva, marco);

    S->tdp[victima].presente = 0;

    S->tdp[nueva].presente = 1;
    S->tdp[nueva].marco = marco;
    S->tdp[nueva].modificada = 0;

    S->tdm[marco].pagina = nueva;
}

void ocupar_marco_libre (ssistema * S, int marco, int pagina)
{
	if(S->detallado)
    	printf ("@ Alojando P%d en M%d\n", pagina, marco);
	
	S->tdm[marco].pagina = pagina;
	S->tdp[pagina].marco = marco;
	S->tdp[pagina].presente = 1;
	S->tdp[pagina].referenciada = 0;
	S->tdp[pagina].modificada = 0;

	// Si la lista está vacia (-1), se apunta listamarcos al marcos, y el sig a si mismo.
	if (S -> listaocupados == -1){
		S -> tdm[marco].sig = marco;
		S -> listaocupados = marco;
	// Si no esta vacia, apuntamos el sig de listamarcos al marco nuevo. El siguiente del marco nuevo apunta al primero. Este ahora es el último.
	} else{
		S -> tdm[marco].sig = S -> tdm[S -> listaocupados].sig;
		S -> tdm[S -> listaocupados].sig = marco;
		S -> listaocupados = marco;
	}

}


// Funciones que muestran resultados

void mostrar_tabla_de_paginas (ssistema * S)
{
    int p;

    printf ("%10s %10s %10s   %s\n",
            "PÁGINA", "Presente", "Marco", "Modificada");

    for (p=0; p<S->numpags; p++)
        if (S->tdp[p].presente)
            printf ("%8d   %6d     %8d   %6d\n", p,
                    S->tdp[p].presente, S->tdp[p].marco,
                    S->tdp[p].modificada);
        else
            printf ("%8d   %6d     %8s   %6s\n", p,
                    S->tdp[p].presente, "-", "-");
}

void mostrar_tabla_de_marcos (ssistema * S)
{
    int p, m;

    printf ("%10s %10s %10s   %s\n",
            "MARCO", "Página", "Presente", "Modificada");

    for (m=0; m<S->nummarcos; m++)
    {
        p = S->tdm[m].pagina;

        if (p==-1)
            printf ("%8d   %8s   %6s     %6s\n", m, "-", "-", "-");
        else if (S->tdp[p].presente)
            printf ("%8d   %8d   %6d     %6d\n", m, p,
                    S->tdp[p].presente, S->tdp[p].modificada);
        else
            printf ("%8d   %8d   %6d     %6s   ¡ERROR!\n", m, p,
                    S->tdp[p].presente, "-");
    }
}

void mostrar_informe_reemplazo (ssistema * S)
{
	int i, marco;

    printf ("Reemplazo Fifo\n");

	printf("Lista de marcos ocupados: \n");
	
	//Iteramos por todos los marcos ocupados, desde el primero al último
	marco = S-> tdm[S-> listaocupados].sig; // Primero
	printf ("%d	", marco);
	for (i=1; i<S->nummarcos; i++){
		marco = S->tdm[marco].sig; 			//Segundo, ...
		printf ("%d	", marco);
	}
}
