# Meta-heurística GRASP para o problema da peregrinação

## Compilação e Execução

Esse projeto faz uso do sistema de compilação [Meson](https://mesonbuild.com/).
Veja o [guia de uso do Meson](https://mesonbuild.com/Quick-guide.html) para informações gerais sobre como obter e usar o Meson.
Também é necessária a instalação de um compilador e biblioteca padrão de C++17.

1. Inicialize o diretório do sistema de compilação (no exemplo, `build`):

```shell
meson setup build --buildtype=release
```

2. Compile o projeto neste diretório:

```shell
meson compile -C build
```

3. Execute o programa gerado:

```shell
./build/grasper [file] [num_iterations] [seed]
```

