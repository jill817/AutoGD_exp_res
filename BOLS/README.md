# BOLS
code of BOLS algorithm

## Data
we now provide the benchmark data in public <https://mega.nz/file/BzFSGTpa#ZkX_DpvcS_9R0PtZMi776LPK1GkbLQkHMM_As0a4fCg>

## Usage
Using ./build.sh to build the solver 

To run the solver, using 

./solver "demand_file_path" "supply_file_path" "time_limit" "parameter_1" "parameter_2"

"parameter_1" "parameter_2" is $t_s$ in the paper


# 修改后运行
./src/solver test/test_demand.txt test/test_supply.txt test/test_heat.txt test/

./src/solver /pub/netdisk1/lijy/AutoGD_exp_data/sample1000/demand/offline/0531_01_10.txt /pub/netdisk1/lijy/AutoGD_exp_data/sample1000/supply/0531_01_10.txt /pub/netdisk1/lijy/AutoGD_exp_data/sample1000/heat/0531_01_10.txt /pub/netdisk1/lijy/Auto_GD/src/examples/BOLS/original_BOLS/test/