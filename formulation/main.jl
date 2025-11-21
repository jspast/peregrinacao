using JuMP
using HiGHS

# ---------- HELPERS ----------
# Euclidian distance between two points p1 and p2
function euclidian_distance(p1::Tuple{Float64,Float64}, p2::Tuple{Float64,Float64})::Float64
    return sqrt((p1[1] - p2[1])^2 + (p1[2] - p2[2])^2)
end

# ---------- MAIN ----------
function main()

    # --- PARSING ENTRIES ---
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
    # We assume the file is well-formed (minimal validation)

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

    println("Instance loaded: T=$T, P=$(length(prerequisites)). \nBuilding matrices...")


    # --- BUILDING FORMULATION ENTRIES ---
    # Create distance matrix (extended for dummy node T+1)
    distance_matrix = Array{Float64}(undef, T+1, T+1)
    for i in 1:T
        distance_matrix[i, i] = 0.0
        for j in (i+1):T
            d = floor(100 * euclidian_distance(temples[i], temples[j]))
            distance_matrix[i, j] = d
            distance_matrix[j, i] = d
        end
        distance_matrix[i, T+1] = 0.0
        distance_matrix[T+1, i] = 0.0
    end
    distance_matrix[T+1, T+1] = 0.0

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

    println("Starting model...")
    

    # --- MODEL DEFINITION ---
    # Defining model
    m = Model(HiGHS.Optimizer)
    set_time_limit_sec(m, max_time)
    set_optimizer_attribute(m, "random_seed", seed)

    # Variables
    # x[i,j] includes dummy node; x[i,j] == 1 if we go from i to j (directed arc)
    @variable(m, x[1:T+1, 1:T+1], Bin)
    # u[i] is MTZ position variable (integer domain tightened by earliest/latest)
    # We don't need to include dummy node in u
    @variable(m, earliest[i] <= u[i=1:T] <= latest[i], Int)

    # Constraints
    # Each node has exactly one outgoing and exactly one incoming arc
    for i in 1:T+1
        @constraint(m, sum(x[i, j] for j in 1:T+1 if i != j) == 1)
        @constraint(m, sum(x[j, i] for j in 1:T+1 if i != j) == 1)
        @constraint(m, x[i, i] == 0) # no self-loops
    end

    # MTZ subtour elimination (big-M style)
    # For every possible arc (i != j), if x[i,j] == 1 then u[j] >= u[i] + 1
    # Ee implement: u[j] >= u[i] + 1 - M*(1 - x[i,j]), with M = T (safe big-M)
    for i in 1:T
        for j in 1:T
            if i != j
                @constraint(m, u[j] >= u[i] + 1 - T * (1 - x[i, j]))

                # OPTIMIZATION 1: 
                # if earliest[i] >= latest[j], then arc i->j is impossible
                if earliest[i] >= latest[j]
                    @constraint(m, x[i,j] == 0)
                end
            end
        end
    end

    # Precedence order
    for i in 1:T
        for j in 1:T
            # For each prerequisite i -> j
            if prereqs_matrix[i, j] == 1
                @constraint(m, u[i] + 1 <= u[j]) # Enforce order through MTZ variables
                @constraint(m, x[j,i] == 0) # Forbid direct arc from i to j
            end

            # OPTIMIZATION 2:
            # For each transitive prerequisite i -> j
            if tc_prereqs_matrix[i, j] == 1
                @constraint(m, x[j,i] == 0) # Forbid arc from j to i
            end
        end
    end

    # Restrictions for dummy node arcs
    # If k has prerequisites, it CANNOT be the first; thus x[T+1, k] = 0
    # If k is a prerequisite for someone, it CANNOT be the last; thus x[k, T+1] = 0
    for k in 1:T
        if earliest[k] > 1
            @constraint(m, x[T+1, k] == 0)
        end
        if latest[k] < T
            @constraint(m, x[k, T+1] == 0)
        end
    end

    # Objective and execution
    @objective(m, Min, sum(distance_matrix[i,j] * x[i,j] for i in 1:T+1, j in 1:T+1))
    optimize!(m)
    @show objective_value(m)

end

main()
