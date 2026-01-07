# 保量广告的RAP算法的实现
参考论文：A Request-level Guaranteed Delivery Advertising Planning: Forecasting and Allocation[https://dl.acm.org/doi/10.1145/3394486.3403348]

# 算法流程


## 完整参数模式
```bash
./RAP <demand_file> <supply_file> <heat_file> <output_base>

# 示例 1
./model_and_RAP /pub/netdisk1/lijy/alimama/competitors/Tencent_RAP/test_data/test_demand_multicomponent.txt /pub/netdisk1/lijy/alimama/competitors/Tencent_RAP/test_data/test_supply_multicomponent.txt /pub/netdisk1/lijy/alimama/competitors/Tencent_RAP/test_data/test_heat_multicomponent.txt /pub/netdisk1/lijy/alimama/competitors/Tencent_RAP/test_data/output/result 

```

## 使用语法
```bash
./RAP <demand_file> <supply_file> <heat_file> <output_base> <time_limit>   
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

### `<time_limit>` (必需)
时间限制参数，控制求解的最大运行时间（单位：秒）。

**示例**:
```
3600
```
- `3600`: 最大运行时间为 3600 秒。

