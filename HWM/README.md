# 保量广告的HWM算法的离线部分实现
参考论文：Ad Serving Using a Compact Allocation Plan[https://arxiv.org/abs/1203.3593]

# 算法流程
HWM离线部分算法流程：
1. 给每个合同$j$定义一个$\alpha_j$, 以及一个分配顺序（分配顺序的排列方式：可用的 supply也就是$S_j$少的demand 排在前面）
2. 在线投放的时候，每个合同都有$\alpha_j$的机会展示，除非出现了所有加起来超出 100% 的情况。如果超限的话，排名在前面的合同先获得访问，排名在后面的合同获得剩余访问，不得超过它的比例。
3. $\alpha_j$的计算方式：
	1. 初始化剩余供应量：对于所有 supply，剩余供应量$r_i$初始化为$s_i$
	2. 对于每一个合同$j$
		1. 求解方程来求得$\alpha_j$:$$\sum_{i \in \Gamma(j)} min(r_i,s_i \cdot \alpha_j) = d_j$$(如果无解则$\alpha_j$=1)
		2. 更新剩余供应量$r_i=r_i-min(r_i,s_i \cdot \alpha_j)$(对于合同 j 对应的这些计划)

## 快速测试模式
```bash
./HWM --test
```
使用预设的测试数据快速验证程序功能，无需指定复杂的参数。

## 完整参数模式（自动连通分支划分）
```bash
# 程序会自动将 supply 文件划分为多个连通分支，并为每个分支单独求解
./HWM <demand_file> <supply_file> <heat_file> <output_base> [online_query_list]

# 示例 1
./HWM /pub/data/lijy/alimama/HWM/test_data/test_demand_multicomponent.txt /pub/data/lijy/alimama/HWM/test_data/test_supply_multicomponent.txt /pub/data/lijy/alimama/HWM/test_data/test_heat_multicomponent.txt /pub/data/lijy/alimama/HWM/test_data/result "1,2,3"

# 示例 2
./HWM /pub/data/lijy/alimama/supply_demand_data/demand/demand_0531.txt /pub/data/lijy/alimama/supply_demand_data/0531/supply_0531_sample100_format.txt /pub/data/lijy/alimama/heat_data/0531_old/heat_supply_0531_sample100_format.txt /pub/data/lijy/alimama/HWM/res/0531/sample100_components/sample100_auto 75434962076,75352456666,75330272698,75329403109

./HWM /pub/data/lijy/alimama/supply_demand_data/demand/demand_0531.txt /pub/data/lijy/alimama/supply_demand_data/0531/supply_0531_sample100_format_components/component_1.txt /pub/data/lijy/alimama/heat_data/0531_old/heat_supply_0531_sample100_format.txt /pub/data/lijy/alimama/HWM/res/0531/sample100_components/com1_Auto 75434962076,75352456666,75330272698,75329403109

./HWM /pub/data/lijy/alimama/supply_demand_data/demand/demand_0531.txt /pub/data/lijy/alimama/supply_demand_data/0531/supply_0531_sample10_format_components/component_4.txt /pub/data/lijy/alimama/heat_data/0531_old/heat_supply_0531_sample10_format.txt /pub/data/lijy/alimama/HWM/res/0531/sample10_components/com4_auto 75434962076,75352456666,75330272698,75329403109

# 示例 3
./HWM /pub/data/lijy/alimama/supply_demand_data/demand/demand_0531_qyt_xianyu_quanyutong_format.txt /pub/data/lijy/alimama/supply_demand_data/0531/supply_qyt_xianyuhigh_format.txt /pub/data/lijy/alimama/heat_data/supply_qyt_xianyuhigh_heat.txt /pub/data/lijy/alimama/HWM/res/0531/qyt_xianyu_components/qyt_xianyu 75377659702,74958077105,75329403109,75031065152,75169249632

# 示例 4
./HWM /pub/data/lijy/alimama/supply_demand_data/demand/demand_330_0416.txt /pub/data/lijy/alimama/supply_demand_data/format_supply/origin_format_supply/supply_0416_connect_format.txt /pub/data/lijy/alimama/heat_data/0416_old/supply_heat_0416_connect.txt /pub/data/lijy/alimama/HWM/res/0416/0416_connect_newAuto 74907066058,74907032072,74882284601,74882226568,74861179083
```

## 使用语法
```bash
./HWM --test                                                    (测试模式)
./HWM <demand_file> <supply_file> <heat_file> <output_base> [online_query_list]    (自动分支模式)
```

## 参数含义和格式说明

### `<demand_file>` (必需)
需求数据文件，描述每个需求ID及其所需的PV数量。

**格式**: 每行包含一个需求记录，使用反引号(`)分隔
```
demand_id`demand_amount
```

**示例**:
```
1`4
2`2
3`2
```
- `1`: 需求ID为1，需要4个PV
- `2`: 需求ID为2，需要2个PV  
- `3`: 需求ID为3，需要2个PV

### `<supply_file>` (必需)
供应数据文件，描述每个用户-供应组合的PV数量和关联的需求列表。

**格式**: 每行包含一个供应记录，使用反引号(`)分隔
```
user_id`supply_id`pv_count`demand_list
```

**示例**:
```
user1`supply1`100`1,2,3
user2`supply2`50`2,3
```
- `user1_supply1`: 用户1的供应1，有100个PV，可以满足需求1,2,3
- `user2_supply2`: 用户2的供应2，有50个PV，可以满足需求2,3

### `<heat_file>` (必需)
热度数据文件，描述每个用户-供应组合的热度系数。

**格式**: 每行包含一个热度记录，使用反引号(`)分隔
```
user_id`supply_id`heat_coefficient
```

**示例**:
```
user1`supply1`0.8
user2`supply2`0.3
```
- 热度系数范围通常在0-1之间，数值越高表示热度越高

### `<output_file>` (必需)
输出文件路径前缀，程序会生成两个文件：
- `{output_file}_offline.csv`: 离线分配结果
- `{output_file}_online.csv`: 在线分配结果

**输出格式**: CSV文件，包含以下列
```
SID,Demand,Allocated PV
user1_supply1,1,50
user1_supply1,2,30
```

### `[online_query_list]` (可选)
在线查询的需求ID列表，用逗号分隔。如果不提供，将使用默认值。

**格式**: 逗号分隔的需求ID字符串
```
demand_id1,demand_id2,demand_id3
```

**示例**:
```
75434962076,75352456666,75330272698,75329403109
```

