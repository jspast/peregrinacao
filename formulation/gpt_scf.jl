using JuMP
using HiGHS
using Printf

function euclidian_distance(p1::Tuple{Float64,Float64}, p2::Tuple{Float64,Float64})::Float64
    return sqrt((p1[1] - p2[1])^2 + (p1[2] - p2[2])^2)
end

function main()
    # Read command line arguments
    if length(ARGS) < 3
        println("Usage: julia sop_pli.jl <arquivo> <max_time> <seed>")
        exit(1)
    end

    input_file = ARGS[1]
    max_time = parse(Float64, ARGS[2])
    seed = parse(Int, ARGS[3])

    # Simple parsing for the expected format:
    # Line 1: integer T
    # Next T lines: two numbers per line (pair of coordinates)
    # Next line: integer P
    # Next P lines: two numbers per line (pair of pre-requisites)
    # We assume the file is well-formed (minimal validation as requested)

    # Read all lines
    lines = readlines(input_file)
    idx = 1

    # Parse T and first block of pairs
    T = parse(Int, strip(lines[idx])); idx += 1
    temples = Vector{Tuple{Float64,Float64}}()
    for k in 1:T
        ln = strip(lines[idx]); idx += 1
        parts = split(ln)
        a = parse(Float64, parts[1])
        b = parse(Float64, parts[2])
        push!(temples, (a, b))
    end

    # Parse P and second block of pairs
    P = parse(Int, strip(lines[idx])); idx += 1
    prerequisites = Vector{Tuple{Float64,Float64}}()
    for k in 1:P
        ln = strip(lines[idx]); idx += 1
        parts = split(ln)
        a = parse(Float64, parts[1])
        b = parse(Float64, parts[2])
        push!(prerequisites, (a, b))
    end

    # Create distance matrix
    distance_matrix = Array{Float64}(undef, T, T)
    for i in 1:T
        distance_matrix[i, i] = 0.0
        for j in (i+1):T
            distance_matrix[i, j] = floor(100 * euclidian_distance(temples[i], temples[j]))
            distance_matrix[j, i] = distance_matrix[i, j]
        end
    end

    # Create transitive closure of prerequisites matrix
    prereqs_matrix = fill(0, T, T)
    for (a, b) in prerequisites
        prereqs_matrix[Int(a), Int(b)] = 1
    end
    tc_prereqs_matrix = copy(prereqs_matrix)
    for k in 1:T
        for i in 1:T
            for j in 1:T
                if tc_prereqs_matrix[i,k] == 1 && tc_prereqs_matrix[k,j] == 1
                    tc_prereqs_matrix[i,j] = 1
                end
            end
        end
    end

    println("Pre processing done. Starting optimization...")


    # ----  EARLIEST / LATEST  ----
    earliest = zeros(Int, T)
    for t in 1:T
        earliest[t] = 1 + count(i -> tc_prereqs_matrix[i,t] == 1, 1:T)
    end

    latest = zeros(Int, T)
    for t in 1:T
        latest[t] = T - count(j -> tc_prereqs_matrix[t,j] == 1, 1:T)
    end


    # ---- DEFINE MODEL ----
    m = Model(HiGHS.Optimizer)
    set_optimizer_attribute(m, "time_limit", max_time)
    set_optimizer_attribute(m, "random_seed", seed)


    # ---- DECISION VARIABLES ----
    # x[i,j] = 1 se j é visitado imediatamente após i
    @variable(m, x[1:T, 1:T], Bin)
    @constraint(m, [i=1:T], x[i,i] == 0)

    # fluxo único (single commodity)
    @variable(m, f[1:T, 1:T] >= 0)


    # ---- DEGREE CONSTRAINTS ----
    @constraint(m, [i=1:T], sum(x[i,j] for j in 1:T) <= 1)   # cada nó tem <= 1 sucessor
    @constraint(m, [j=1:T], sum(x[i,j] for i in 1:T) <= 1)   # cada nó tem <= 1 predecessor
    @constraint(m, sum(x) == T-1)

    # ---- PRECEDÊNCIA: proibir arcos inválidos ----
    for i in 1:T, j in 1:T
        if tc_prereqs_matrix[j,i] == 1
            @constraint(m, x[i,j] == 0)     # não pode ir de i para j
        end
    end

    # ---- OTIMIZAÇÃO A2: eliminar arcos que quebram earliest/latest ----
    for i in 1:T, j in 1:T
        if earliest[j] > latest[i] + 1
            @constraint(m, x[i,j] == 0)
        end
        if earliest[i] >= latest[j]
            @constraint(m, x[i,j] == 0)
        end
    end

    # ---- OTIMIZAÇÃO A1 + A3: SINGLE COMMODITY FLOW ----

    # Capacidade baseada em janelas, não em T
    cap = maximum(latest[i] - earliest[i] for i in 1:T)

    # fluxo só pode passar em arcos ativos
    @constraint(m, [i=1:T, j=1:T], f[i,j] <= cap * x[i,j])

    # escolha automática de um nó inicial e final:
    # in_degree = 0 → candidato a início
    # out_degree = 0 → candidato a fim
    possible_start = findall(t -> count(i -> tc_prereqs_matrix[i,t] == 1, 1:T) == 0, 1:T)
    possible_end   = findall(t -> count(j -> tc_prereqs_matrix[t,j] == 1, 1:T) == 0, 1:T)

    start = length(possible_start) == 1 ? possible_start[1] : nothing
    finish = length(possible_end) == 1 ? possible_end[1] : nothing

    # balanço de fluxo
    for v in 1:T
        if start !== nothing && v == start
            @constraint(m, sum(f[v,j] for j in 1:T) - sum(f[i,v] for i in 1:T) == 1)
        elseif finish !== nothing && v == finish
            @constraint(m, sum(f[v,j] for j in 1:T) - sum(f[i,v] for i in 1:T) == -1)
        else
            @constraint(m, sum(f[v,j] for j in 1:T) - sum(f[i,v] for i in 1:T) == 0)
        end
    end


    # ---- OBJETIVO ----
    @objective(m, Min, sum(distance_matrix[i,j] * x[i,j] for i in 1:T, j in 1:T))

    optimize!(m)
    @show objective_value(m)


end

main()