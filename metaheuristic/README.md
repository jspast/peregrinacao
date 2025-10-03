# Meta-heurística GRASP para o problema da peregrinação

## Compilação e Execução

Esse projeto foi desenvolvido em C++17, utilizando apenas recursos da biblioteca padrão da linguagem.
Portanto, é necessária a instalação de um compilador e biblioteca padrão de C++17.

1. Compile o projeto com otimizações para a microarquitetura do seu computador:

```shell
g++ grasper.cpp -std=c++17 -O3 -march=native -o grasper
```

2. Execute o programa gerado:

```shell
./grasper <file> <num_iterations> <seed> [alpha] [time_control]
```
