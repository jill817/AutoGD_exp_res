# 运行方式

## ⭐ 新功能：自动连通分支划分
程序现在会自动对输入的供应数据进行连通分支划分，并为每个独立的分支单独进行优化求解。

## 快速测试模式
```bash
./xunliang-ls --test
```
使用预设的测试数据快速验证程序功能，无需指定复杂的参数。

## 完整参数模式（自动连通分支划分）
```bash
# 程序会自动将 supply 文件划分为多个连通分支，并为每个分支单独求解
./xunliang-ls <demand_file> <supply_file> <heat_file> <output_base> [online_query_list]

# 示例 1
./xunliang-ls /pub/netdisk1/lijy/alimama/competitors/Local_xunliang/test_data/test_demand_multicomponent.txt /pub/netdisk1/lijy/alimama/competitors/Local_xunliang/test_data/test_supply_multicomponent.txt /pub/netdisk1/lijy/alimama/competitors/Local_xunliang/test_data/test_heat_multicomponent.txt /pub/netdisk1/lijy/alimama/competitors/Local_xunliang/test_data/result "1,2,3"

# 示例 2
./xunliang-ls /pub/netdisk1/lijy/alimama/supply_demand_data/demand/demand_0531.txt /pub/netdisk1/lijy/alimama/supply_demand_data/0531/supply_0531_sample100_format.txt /pub/netdisk1/lijy/alimama/heat_data/0531_old/heat_supply_0531_sample100_format.txt /pub/netdisk1/lijy/alimama/competitors/Local_xunliang/res/0531/sample100_components/sample100_auto 75434962076,75352456666,75330272698,75329403109

./xunliang-ls /pub/netdisk1/lijy/alimama/supply_demand_data/demand/demand_0531.txt /pub/netdisk1/lijy/alimama/supply_demand_data/0531/supply_0531_sample100_format_components/component_1.txt /pub/netdisk1/lijy/alimama/heat_data/0531_old/heat_supply_0531_sample100_format.txt /pub/netdisk1/lijy/alimama/competitors/Local_xunliang/res/0531/sample100_components/com1_Auto 75434962076,75352456666,75330272698,75329403109

./xunliang-ls /pub/netdisk1/lijy/alimama/supply_demand_data/demand/demand_0531.txt /pub/netdisk1/lijy/alimama/supply_demand_data/0531/supply_0531_sample10_format_components/component_4.txt /pub/netdisk1/lijy/alimama/heat_data/0531_old/heat_supply_0531_sample10_format.txt /pub/netdisk1/lijy/alimama/competitors/Local_xunliang/res/0531/sample10_components/com4_auto 75434962076,75352456666,75330272698,75329403109

# 示例 3
./xunliang-ls /pub/netdisk1/lijy/alimama/supply_demand_data/demand/demand_0531_qyt_xianyu_quanyutong_format.txt /pub/netdisk1/lijy/alimama/supply_demand_data/0531/supply_qyt_xianyuhigh_format.txt /pub/netdisk1/lijy/alimama/heat_data/supply_qyt_xianyuhigh_heat.txt /pub/netdisk1/lijy/alimama/competitors/Local_xunliang/res/0531/qyt_xianyu_components/qyt_xianyu 75377659702,74958077105,75329403109,75031065152,75169249632

# 示例 4
./xunliang-ls /pub/netdisk1/lijy/alimama/supply_demand_data/demand/demand_330_0416.txt /pub/netdisk1/lijy/alimama/supply_demand_data/format_supply/origin_format_supply/supply_0416_connect_format.txt /pub/netdisk1/lijy/alimama/heat_data/0416_old/supply_heat_0416_connect.txt /pub/netdisk1/lijy/alimama/competitors/Local_xunliang/res/0416/0416_connect_newAuto 74907066058,74907032072,74882284601,74882226568,74861179083
```

## 使用语法
```bash
./xunliang-ls --test                                                    (测试模式)
./xunliang-ls <demand_file> <supply_file> <heat_file> <output_base> [online_query_list]    (自动分支模式)
```

## 新工作流程说明

### 自动连通分支划分流程
1. **输入处理**：读取原始的 supply 文件
2. **连通分支检测**：自动识别二分图中的所有连通分支
3. **分支文件生成**：为每个连通分支生成独立的 supply 文件
4. **独立求解**：对每个分支单独进行资源分配优化
5. **结果输出**：生成每个分支的独立结果文件

### 输出文件命名规则
- 输入 output_base 为 `/path/to/result`
- 自动生成的分支结果文件：
  - `/path/to/result_component_1_offline.csv` 和 `/path/to/result_component_1_online.csv`
  - `/path/to/result_component_2_offline.csv` 和 `/path/to/result_component_2_online.csv`  
  - 等等...
./xunliang-ls <demand_file> <supply_file> <heat_file> <output_file> [online_query_list]    (完整模式)
```

## 测试模式说明

### 使用测试模式
```bash
./xunliang-ls --test
```

测试模式会使用预设的测试数据进行快速验证：
- **需求文件**: `/pub/netdisk1/lijy/alimama/competitors/Local_xunliang/test_data/demand.txt`
- **供应文件**: `/pub/netdisk1/lijy/alimama/competitors/Local_xunliang/test_data/supply.txt`
- **热度文件**: `/pub/netdisk1/lijy/alimama/competitors/Local_xunliang/test_data/heat.txt`
- **输出路径**: `/pub/netdisk1/lijy/alimama/competitors/Local_xunliang/test_data/test`
- **在线查询**: `["1", "2"]`

测试模式的优势：
- 无需准备复杂的输入参数
- 使用小规模数据，运行速度快
- 验证程序基本功能是否正常
- 检查并行算法是否工作正确

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

