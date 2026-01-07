# PILLS-offline 


## 编译

1. 进入源码目录并编译：

```bash
cd src
make
```

2. 编译成功后会在 `src/` 下生成可执行文件 `main`。

注意：makefile 使用 `g++ -std=c++20 -O3` 并在链接时依赖 `-lboost_thread -lboost_system -lpthread`。

## 运行用法

可执行文件位于 `src/main`（如果你在 `src/` 目录下直接运行，可用 `./main`）：

```
./main --test
或
./main <demand_file> <supply_file> <heat_file> <output_base> [online_query_list]
```

参数说明：

- `--test`：快速测试模式。程序会使用内置的示例路径并输出测试结果（用于功能验证）。

- `<demand_file>`（必需）：需求文件路径。
	- 格式（每行用反引号 ` 分隔）：
		```
		demand_id`demand_amount
		```
	- 示例：
		```
		1`4
		2`2
		3`2
		```

- `<supply_file>`（必需）：已格式化的供应文件路径。
	- 格式（每行用反引号 ` 分隔）：
		```
		user_id`supply_id`pv_count`demand_list
		```
	- 其中 `demand_list` 为以逗号分隔的 demand_id 列表，例如 `1,2,3`。
	- 示例：
		```
		user1`supply1`100`1,2,3
		user2`supply2`50`2,3
		```

- `<heat_file>`（必需）：热度系数文件路径。
	- 格式（每行用反引号 ` 分隔）：
		```
		user_id`supply_id`heat_coefficient
		```
	- 示例：
		```
		user1`supply1`0.8
		user2`supply2`0.3
		```

- `<output_base>`（必需）：输出文件路径前缀（程序会基于该前缀生成若干结果文件）。

- `[online_query_list]`（可选）：逗号分隔的需求ID列表，用于在线查询/优先处理。若不提供程序会使用默认值（源码中有默认 ID 列表）。

## 输出

程序会对输入的 `supply_file` 自动进行连通分支划分（每个分支单独求解），并为每个分支生成对应的离线/在线结果文件，命名规则示例（若 `output_base` 为 `/path/to/result`）：

- `/path/to/result_component_1_offline.csv`
- `/path/to/result_component_1_online.csv`
- `/path/to/result_component_2_offline.csv`
- `/path/to/result_component_2_online.csv`

此外，若不做连通分支拆分（或输入本身为单组件），也会生成以 `<output_base>_offline.csv` 和 `<output_base>_online.csv` 为后缀的文件。

输出 CSV 中包含类似如下列（示例）：

```
SID,Demand,Allocated PV
user1_supply1,1,50
user1_supply1,2,30
```


