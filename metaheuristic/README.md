# Meta-heurística GRASP para o problema da peregrinação

## Compilação e Execução

Esse projeto foi desenvolvido em C++20, utilizando apenas recursos da biblioteca padrão da linguagem.
Portanto, é necessária a instalação de um compilador e biblioteca padrão de C++20.

1. Compile o projeto com otimizações para a microarquitetura do seu computador:

```shell
g++ grasper.cpp -std=c++20 -O3 -march=native -o grasper -Wall -Wextra
```

2. Execute o programa gerado:

```
grasper <file> [iterations_limit] [seed] [ADDITIONAL_OPTIONS]
```

Exemplo:
```
grasper 01.txt 50000 1 -a=0.2
```

Opções adicionais:
```
-a, --alpha=FLOAT
-t, --time-limit=FLOAT
    --generate-results
```

Para usar a flag `--generate-results`, \<file> deve ser um diretório com dez instâncias do problema em arquivos nomeados "01.txt", "02.txt" ... "10.txt". Os resultados são exportados em arquivos CSV com nome "01.csv", "02.csv" ... "10.csv" e colunas `tempo_alvo,alpha,seed,iterações_parciais,iterações_grasp,tempo_execução,valor`, onde
- tempo_alvo: 5 ou 300 segundos
- alpha: 0.05 ou 0.2
- seed: 1 a 5
- iterações_parciais: iterações individuais da busca local e da construção gulosa-aleatória
- iterações_grasp: execuções do GRASP como um todo
- tempo_execução: tempo real de execução, em segundos
- valor: valor da melhor solução encontrada
