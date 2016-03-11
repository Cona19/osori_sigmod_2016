import networkx as nx
import random

from random import shuffle

#see the details at https://networkx.github.io/documentation/latest/reference/generated/networkx.readwrite.edgelist.write_edgelist.html#networkx.readwrite.edgelist.write_edgelist
#see the random graph types at https://networkx.github.io/documentation/latest/reference/generators.html

#graph configuration factors
node_num = 2000
edge_prob = 0.2

init_file = open('init-file.txt', 'wb')
workload_file = open('workload-file.txt', 'wb')

init_file_ratio = 0.5
query_prob= 0.35
delete_prob= 0.4

#generate random graph - this can be changed to another algorithm
G = nx.fast_gnp_random_graph(node_num, edge_prob,directed=1)
edge_list = nx.to_edgelist(G)

#shuffle command occasionally makes some error. so 'sorted' is better way to solve.
#shuffle(edge_list)
edge_list = sorted(iter(edge_list), key=lambda k: random.random())

#make init_edge_list
init_edge_list = edge_list[0:int(len(edge_list) * init_file_ratio)]
workload_edge_list = []

#fix the add commands
for edge in edge_list[int(len(edge_list) * init_file_ratio):len(edge_list)]:
	workload_edge_list.append((edge[0], edge[1], 'A'))


#make query commands and delete commands according to the probability
i = 0
cnt_add = 0
while i < len(workload_edge_list):
	if random.random() < query_prob:
		workload_edge_list.insert(i, [random.randrange(0, node_num), random.randrange(0, node_num), 'Q'])
		i += 1
		continue
	if random.random() < delete_prob:
		rv = random.randrange(0, len(init_edge_list) + cnt_add);
		workload_edge_list.insert(i, [edge_list[rv][0], edge_list[rv][1],'D']);
		i += 1
		continue

	cnt_add += 1
	i += 1

#print result to init_file and workload_file
for edge in init_edge_list:
	print >> init_file, str(edge[0]) + ' ' + str(edge[1])

for edge in workload_edge_list:
	print >> workload_file, edge[2] + ' ' + str(edge[0]) + ' ' + str(edge[1])

#close files
init_file.close()
workload_file.close()
