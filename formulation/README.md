# Formulação inteira para o problema da peregrinação

## Compilação e Execução

Este projeto foi desenvolvido em Julia 1.9+, utilizando a biblioteca de modelagem JuMP e o solver HiGHS.
Para executar o programa, é necessário ter Julia instalada e instalar os pacotes usados:
```julia
import Pkg
Pkg.add(["JuMP", "HiGHS"])
```

Execute:
```shell
julia main.jl <file> <time_limit> <seed>
```

Exemplo:
``` shell
julia main.jl 01.txt 300 1
```

Argumentos
- <file> — arquivo de entrada descrevendo templos e pré-requisitos
- <time_limit> — tempo máximo do solver, em segundos (Float)
- <seed> — semente numérica para o algoritmo (Int)


## Formato do arquivo de entrada

O formato esperado contém:
- na linha 1: um inteiro T;
- nas próximas T linhas: dois números por linha (par de coordenadas);
- na linha T+2: um inteiro P;
- nas próximas P linhas: dois números por linha (par de pré-requisitos).

Exemplo estrutural:
```txt
T
x1 y1
x2 y2
...
xT yT
P
a1 b1
a2 b2
...
aP bP
```

## Técnica aplicada

O solver encontra um caminho dirigido que:
1. Visita todos os templos exatamente uma vez
2. Tem T − 1 arestas
3. Respeita todas as relações de precedência
4. Minimiza a distância total percorrida no plano
5. Elimina ciclos por:
    - Proibição direta de arcos inválidos
    - Formulação MTZ reforçada com limites earliest/latest

As distâncias são euclidianas multiplicadas por 100 e arredondadas para baixo.