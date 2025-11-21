using JuMP
using HiGHS
using Printf

function euclidian_distance(p1::Tuple{Float64,Float64}, p2::Tuple{Float64,Float64})::Float64
    return sqrt((p1[1] - p2[1])^2 + (p1[2] - p2[2])^2)
end

function main()
    # Read command line arguments
    if length(ARGS) < 3
        println("Usage: julia main.jl <input_file> <max_time> <seed>")
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
    prerequisites = Vector{Tuple{Int,Int}}()
    for k in 1:P
        ln = strip(lines[idx]); idx += 1
        parts = split(ln)
        a = parse(Int, parts[1])
        b = parse(Int, parts[2])
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
        prereqs_matrix[a, b] = 1
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

    # Compute earliest and latest positions for each temple
    earliest = zeros(Int, T)
    for t in 1:T
        earliest[t] = 1 + count(i -> tc_prereqs_matrix[i,t] == 1, 1:T)
    end
    latest = zeros(Int, T)
    for t in 1:T
        latest[t] = T - count(j -> tc_prereqs_matrix[t,j] == 1, 1:T)
    end

    # Detect cycle via inconsistent earliest/latest bounds
    for t in 1:T
        if earliest[t] > latest[t]
            println("Error: prerequisites contain a cycle involving temple $t.")
            println("earliest[$t] = $(earliest[t]), latest[$t] = $(latest[t])")
            exit(1)
        end
    end

    println("Instance loaded: T=$T, P=$(length(prerequisites)). Starting model...")
    

    # Defining model
    m = Model(HiGHS.Optimizer)
    set_time_limit_sec(m, max_time)
    set_optimizer_attribute(m, "random_seed", seed)

    # Variables
    # x[i,j] == 1 if we go from i to j (directed arc)
    @variable(m, x[1:T,1:T], Bin)
    # u[i] is MTZ position variable (integer domain tightened by earliest/latest)
    @variable(m, earliest[i] <= u[i=1:T] <= latest[i], Int)
    
    # Constraints
    # no self-loops
    for i in 1:T
        @constraint(m, x[i,i] == 0)
    end
    # each node has at most one outgoing and at most one incoming arc
    for i in 1:T
        @constraint(m, sum(x[i,j] for j in 1:T) <= 1)   # outdegree <= 1
        @constraint(m, sum(x[j,i] for j in 1:T) <= 1)   # indegree <= 1
    end
    # total number of arcs must be T-1 (path visiting all nodes)
    @constraint(m, sum(x[i,j] for i in 1:T, j in 1:T) == T-1)
    # MTZ subtour elimination (big-M style)
    # for every possible arc (i != j), if x[i,j] == 1 then u[j] >= u[i] + 1
    # we implement: u[j] >= u[i] + 1 - M*(1 - x[i,j]), with M = T (safe big-M)
    for i in 1:T
        for j in 1:T
            if i != j
                @constraint(m, u[j] >= u[i] + 1 - T * (1 - x[i,j]))
            end
        end
    end
    # to ensure correcteness of the path, enforce prerequisites through MTZ variables
    for a in 1:T
        for b in 1:T
            if prereqs_matrix[a,b] == 1
                @constraint(m, u[a] + 1 <= u[b])
            end
        end
    end

    # OPTM 1: if a must come before b (tc_prereqs_matrix[a,b] == 1), then arc b->a is forbidden
    for a in 1:T
        for b in 1:T
            if tc_prereqs_matrix[a,b] == 1
                @constraint(m, x[b,a] == 0)
            end
        end
    end
    
    # OPTM 2: if earliest[i] >= latest[j], then arc i->j is impossible
    for i in 1:T
        for j in 1:T
            if earliest[i] >= latest[j]
                @constraint(m, x[i,j] == 0)
            end
        end
    end

    # Objective and execution
    @objective(m, Min, sum(distance_matrix[i,j] * x[i,j] for i in 1:T, j in 1:T))
    optimize!(m)
    @show objective_value(m)

    # -----------------------------------------------------
    # RECONSTRUCT AND PRINT THE PATH
    # -----------------------------------------------------

    # Retrieve x values in matrix form
    x_val = value.(x)

    # Identify starting node (the one with no incoming arc)
    start_node = nothing
    for i in 1:T
        incoming = sum(x_val[j,i] for j in 1:T)
        if incoming < 0.5
            start_node = i
            break
        end
    end

    if start_node === nothing
        println("Error: no start node found. Something is wrong with the solution.")
        return
    end

    println("\n===== PATH RECONSTRUCTION =====")
    println("Start node = $start_node\n")

    # Follow the path
    path = [start_node]
    current = start_node
    total_dist = 0.0

    for step in 1:(T-1)
        next_node = findfirst(j -> x_val[current,j] > 0.5, 1:T)
        if next_node === nothing
            println("Error: path ended prematurely at node $current.")
            break
        end
        push!(path, next_node)
        total_dist += distance_matrix[current, next_node]
        current = next_node
    end

    println("Visited order: ", path)
    println("Total distance (sum of arcs): ", total_dist)

    # Print detailed arc-by-arc distances
    println("\nArc details:")
    for i in 1:(length(path)-1)
        a = path[i]
        b = path[i+1]
        println("  $a -> $b   (dist = $(distance_matrix[a,b]))")
    end

    println("\nConsistency check:")
    println("  Model objective = ", objective_value(m))
    println("  Reconstructed sum = ", total_dist)


end

main()
