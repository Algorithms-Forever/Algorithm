#pragma warning( disable : 4996)
#include <iostream>
#include <string>
#include <cmath>
#include <ctime>
#include <mysql.h>

constexpr int MaxPlace = 200;//最大城市数
//constexpr int Q = 10;//信息素相关的常数值  这个不用，增加全局性，用当前最优的路径长度作为信息素相关变量
constexpr int RoundAntNum = 20;//每轮使用蚂蚁数

using namespace std;

struct City//顶点
{
	string name;//名称
	int x_place=0, y_place=0;//坐标
	int number=0;//编号
	int demand = 0;//需求
};

struct Graph //地图 邻接矩阵存储
{
	double** adjacentMatrix = new double* [MaxPlace],      //邻接矩阵
		** inspiringMatrix = new double* [MaxPlace],             //启发因子矩阵
		** pheromoneMatrix = new double* [MaxPlace];        //信息素矩阵

	City *ver = new City[MaxPlace];//顶点集
	int num_ver = 0;//顶点个数
};

struct Ant
{
	int cityNumber = 0;//当前所处城市编号
	int* passPath = new int[2 * MaxPlace];//路径轨迹，存经过的城市编号，用来后面走完全程给信息素确认
	int surplusCapacity = 0;//剩余容量
	
	double sumLength=0;//走过的总路径长度
	int count = 0;//记录已走过的城市数
};

//建立连接
bool ConnectSQL(MYSQL *mysql, int mode)
{
	const char* host = "127.0.0.1";
	const char* user = "root";
	const char* pass = "123456";
	const char* db;
	if (mode == 1)  db = "ACO";
	else db = "inputData";

	if (!mysql_real_connect(mysql, host, user, pass, db, 3306, 0, 0))
		return false ;
	return  true;
}

//释放连接
void FreeConnect(MYSQL *mysql)//释放连接
{
	mysql_close(mysql);
}

//创建表
void CreateTables (Graph*& G, MYSQL *mysql, int paths)
{
	string ifExist = "DROP TABLE IF EXISTS `";

	//创建place 表
	string placeTableName = to_string(G->num_ver) +"place";
	string ifExistPlace = ifExist + placeTableName + "`;";
	string createStatementPlace = "CREATE TABLE `" + placeTableName + "`  (\
		`id_num` int(0) NOT NULL,\
		`x_place` int(0) NOT NULL,\
		`y_place` int(0) NOT NULL,\
		`demand` int(0) NOT NULL,\
		PRIMARY KEY(`id_num`) USING BTREE\
		) ENGINE = InnoDB CHARACTER SET = utf8 COLLATE = utf8_general_ci ROW_FORMAT = Dynamic; ";

	mysql_query(mysql, ifExistPlace.c_str());
	if (mysql_query(mysql, createStatementPlace.c_str())) cout << "Create Table " + placeTableName + " defeated!" << endl;

	//创建邻接矩阵，信息素表
	string* vers = new string[G->num_ver];
	for (int i = 0; i < G->num_ver; i++)
		vers[i] = "toVertex" + to_string(i);

	string adjacentMatrixTableName = to_string(G->num_ver) + "adjacentMatrix";
	string pheromoneMatrixTableName = to_string(G->num_ver) + "pheromoneMatrix";

	string ifExistAdjacent = ifExist + adjacentMatrixTableName + "`;";
	string ifExistpheromone = ifExist + pheromoneMatrixTableName + "`;";

	string createStatementAdjacent = "CREATE TABLE `" + adjacentMatrixTableName + "`  (  `ver_num` int(0) NOT NULL ,";
	string createStatementPheromone = "CREATE TABLE `" + pheromoneMatrixTableName + "`  (  `ver_num` int(0) NOT NULL ,";
	for (int i = 0; i < G->num_ver; i++)
	{
		createStatementAdjacent += "`" + vers[i] + "` double(10, 5) NOT NULL,";
		createStatementPheromone += "`" + vers[i] + "` double(10, 5) NOT NULL,";
	}
	createStatementAdjacent += "PRIMARY KEY (`ver_num`) USING BTREE\
		) ENGINE = InnoDB CHARACTER SET = utf8 COLLATE = utf8_general_ci ROW_FORMAT = Dynamic; ";
	createStatementPheromone += "PRIMARY KEY (`ver_num`) USING BTREE\
		) ENGINE = InnoDB CHARACTER SET = utf8 COLLATE = utf8_general_ci ROW_FORMAT = Dynamic; ";

	mysql_query(mysql, ifExistAdjacent.c_str());
	if(mysql_query(mysql, createStatementAdjacent.c_str())) cout << "Create Table " + adjacentMatrixTableName + " defeated!" << endl;
	mysql_query(mysql, ifExistpheromone.c_str());
	if(mysql_query(mysql, createStatementPheromone.c_str())) cout << "Create Table " + pheromoneMatrixTableName + " defeated!" << endl;

	//创建蚂蚁 每代的最优路径表path
	string pathTableName = to_string(G->num_ver) + "path";
	string ifExistPath = ifExist + pathTableName + "`;";

	string createStatementPath = "CREATE TABLE `" + pathTableName + "`  (  `times` int(0) NOT NULL COMMENT '记录代次',";
	for (int i = 0; i < paths; i++)
		createStatementPath += "`path" + to_string(i) + "`  tinytext CHARACTER SET utf8 COLLATE utf8_general_ci NULL,";

	createStatementPath += "`length` int(0) NOT NULL,\
		PRIMARY KEY (`times`) USING BTREE\
		) ENGINE = InnoDB CHARACTER SET = utf8 COLLATE = utf8_general_ci ROW_FORMAT = Dynamic; ";

	mysql_query(mysql, ifExistPath.c_str());
	if(mysql_query(mysql, createStatementPath.c_str())) cout << "Create Table " + pathTableName + " defeated!" << endl;
}

//插入城市相关数据
void InsertTablePlace(Graph*& G, MYSQL *mysql)
{
	string TableName = to_string(G->num_ver) + "place";
	string InsertStatement = "INSERT INTO `" + TableName + "` VALUES (";
	for (int i = 0; i < G->num_ver; i++)
	{
		InsertStatement = InsertStatement + to_string(G->ver[i].number) + "," + to_string(G->ver[i].x_place) + "," + to_string(G->ver[i].y_place) + "," + to_string(G->ver[i].demand) + ");";
		if (mysql_query(mysql, InsertStatement.c_str())) cout << "Insert to place table record " + to_string(i + 1) + " defeated!" << endl;
		InsertStatement = "INSERT INTO `" + TableName + "` VALUES (";
	}
}

//插入矩阵相关信息  邻接矩阵  信息素矩阵
void InsertTableMatrix(Graph*& G, MYSQL *mysql)
{
	string adjacentMatrixTableName = to_string(G->num_ver) + "adjacentMatrix";
	string pheromoneMatrixTableName = to_string(G->num_ver) + "pheromoneMatrix";

	string InsertStatementad = "INSERT INTO `" + adjacentMatrixTableName + "` VALUES (";
	string InsertStatementph = "INSERT INTO `" + pheromoneMatrixTableName + "` VALUES (";

	for (int i = 0; i < G->num_ver; i++)
	{
		InsertStatementad = InsertStatementad + to_string(i) + ",";
		InsertStatementph = InsertStatementph + to_string(i) + ",";
		for (int j = 0; j < G->num_ver; j++)
		{
			if (j != G->num_ver - 1) {
				InsertStatementad = InsertStatementad + to_string(G->adjacentMatrix[i][j]) + ",";
				InsertStatementph = InsertStatementph + to_string(G->pheromoneMatrix[i][j]) + ",";
			}
			else {
				InsertStatementad = InsertStatementad + to_string(G->adjacentMatrix[i][j]) + ");";
				InsertStatementph = InsertStatementph + to_string(G->pheromoneMatrix[i][j]) + ")";
			}
		}
		if(mysql_query(mysql, InsertStatementad.c_str())) cout << "Insert to place adjacent record " + to_string(i + 1) + " defeated!" << endl;
		if(mysql_query(mysql, InsertStatementph.c_str())) cout << "Insert to place pheromone record " + to_string(i + 1) + " defeated!" << endl;;
		InsertStatementad = "INSERT INTO `" + adjacentMatrixTableName + "` VALUES (";
		InsertStatementph = "INSERT INTO `" + pheromoneMatrixTableName + "` VALUES (";
	}
}

//矩阵归一化 Matrix是矩阵 length是矩阵大小 scope是倍率
void MatrixNormalization(double** Matrix, int length, int scope)
{
	double max = -1;
	for (int i = 0; i < length; i++)
		for (int j = i; j < length; j++)
			if (Matrix[i][j] > max) max = Matrix[i][j];

	for (int i = 0; i < length; i++)
		for (int j = 0; j < length; j++)
		{
			Matrix[i][j] = Matrix[i][j] * scope / max;
		}
}

//依据现有信息，创建图
void CreateGraph(Graph*& G, int num_city, string* cityName, double** Matrix,int *demand, int* x_place, int* y_place)
{
	for (int i = 0; i < num_city; i++)//给图的各顶点加上编号和地址,需求
	{
		G->ver[i].name = cityName[i];
		G->ver[i].number = i;
		G->ver[i].x_place = x_place[i];
		G->ver[i].y_place = y_place[i];
		G->ver[i].demand = demand[i];
	}

	//图的初始化
	for (int i = 0; i < num_city; i++)
	{
		G->adjacentMatrix[i] = new double[num_city];
		G->inspiringMatrix[i] = new double[num_city];
		G->pheromoneMatrix[i] = new double[num_city];
	}

	//图的各矩阵赋值
	for (int i = 0; i < num_city; i++)
		for (int j = i ; j < num_city; j++)
		{
			G->adjacentMatrix[j][i] = G->adjacentMatrix[i][j] = int(sqrt(pow(x_place[i] - x_place[j], 2) + pow(y_place[i] - y_place[j], 2)));//邻接矩阵
			G->inspiringMatrix[j][i] = G->inspiringMatrix[i][j] = (Matrix[i][j] != 0 ? 1 / Matrix[i][j] : 0);//启发因子矩阵
			G->pheromoneMatrix[j][i] = G->pheromoneMatrix[i][j] = (i == j ? 0 : 1);//信息素矩阵
		}
	G->num_ver = num_city;
}

//判断元素a是否在集合array中
bool IsInSet(int a, int* array,int num)
{
	for (int i = 0; i < num; i++)
		if (array[i] == a) return true;
	return false;
}

//蚂蚁移动，类似于拓扑排序算法
void AntMove(Graph*& G,  Ant*& ant, int CarCapacity)
{
	//先选择合适的路径，即状态转移规则,暂定 a=1，b=1
	double* probability = new double[G->num_ver], sum = 0;
	int a = 1,b = 1;
	for (int i = 0; i < G->num_ver ; i++)
	{
		//有可能下一步过去的点，以及可能性大小
		if (!IsInSet(i, ant->passPath, ant->count) && G->ver[i].demand <= ant->surplusCapacity)
			probability[i] = pow(G->pheromoneMatrix[ant->cityNumber][i], a) *
				pow(G->inspiringMatrix[ant->cityNumber][i], b);
		else probability[i] = 0;
		sum += probability[i];
	}
	
	if (sum == 0)//即没有一个点符合要求
	{
		//情况一：所有的点都已经走过了，
		//这个时候应当是在主函数里返回，所以这里不会有这种情况出现

		//情况二：还有点没走过，但是当前蚂蚁剩余容量不够了
		ant->count++;//走过城市数加一
		ant->sumLength += G->adjacentMatrix[ant->cityNumber][0];//计算当前总长度
		ant->passPath[ant->count-1] = 0;//写入经过路径
		ant->cityNumber = 0;//修改当前位置
		ant->surplusCapacity = CarCapacity;//修改当前蚂蚁剩余容量
		AntMove(G, ant, CarCapacity);//因为当前这步回来了，所以，这步不算，再走一步
		return;
	}

	//生成随机值 0 0.1 0.2 0.3 0.05
	double RandomNum = (rand() % 100 / double(100)) * sum;
	int choose = -1;//选中节点的编号

	for (choose; choose < G->num_ver && RandomNum>=0; )//确定选中的编号
		RandomNum -= probability[++choose];

	//确定下一步往哪后，对蚂蚁的属性进行修改，以记录路径
	ant->count++;//走过城市数加一
	ant->sumLength += G->adjacentMatrix[ant->cityNumber][choose];//计算当前总长度
	ant->passPath[ant->count-1] = choose;//写入经过路径
	ant->cityNumber = choose;//修改当前位置
	ant->surplusCapacity -= G->ver[choose].demand;//修改蚂蚁剩余容量
	return;
}

//生成蚂蚁路线
string* AntPath(Ant*& ant, int paths)
{
	string* path = new string[paths+1];
	int num = 0;
	path[num] = to_string(ant->passPath[0]) + " ";
	for (int i = 1; i < ant->count-1; i++)
	{
		if (ant->passPath[i] != 0) path[num] = path[num] + to_string(ant->passPath[i]) + " ";
		else {
			path[num] += to_string(ant->passPath[i]);
			num++;
			path[num] = to_string(ant->passPath[i]) + " ";
		}
	}
	path[num] += to_string(ant->passPath[0]);
	return path;
}

//本轮最优路径存储到数据库
void InsertPath(MYSQL* mysql, Graph*& G, Ant*& ant, int time, string* path, int paths)
{
	//time path[i] length
	string antPathTableName = to_string(G->num_ver) + "path";
	string InsertStatement = "INSERT INTO `" + antPathTableName + "` VALUES (" + to_string(time) +  ",";
	for (int i = 0; i < paths; i++)
		InsertStatement = InsertStatement + "\"" + path[i] + "\",";
	InsertStatement = InsertStatement + to_string(ant->sumLength) + ");";
	if (mysql_query(mysql, InsertStatement.c_str())) cout << "Insert to Path table record " + to_string(time) + " defeated!" << endl;
}

//更新信息素矩阵 加入新的差量，并总体蒸发
void updateMatrix(Graph*& G, Ant* ant[],Ant* bestAnt)
{
	double** Matrix;//Δ信息素矩阵
	Matrix = new double* [G->num_ver];
	for (int i = 0; i < G->num_ver; i++)//初始化
	{
		Matrix[i] = new double[G->num_ver];
		for (int j = 0; j < G->num_ver; j++)
			Matrix[i][j] = 0;
	}

	for (int k = 0; k < RoundAntNum; k++)
	{
		double increment = bestAnt->sumLength / (ant[k]->sumLength * RoundAntNum);
		for (int i = 0; i < ant[k]->count - 2; i++)//计算差量
		{
			Matrix[ant[k]->passPath[i]][ant[k]->passPath[i + 1]] += increment;
		}
		Matrix[ant[k]->passPath[ant[k]->count-2]][ant[k]->passPath[0]] += increment;
	}

	for (int i = 0; i < G->num_ver; i++)
		for (int j = 0; j < G->num_ver; j++) 
		{
			G->pheromoneMatrix[i][j] *= 0.7;//蒸发因子为0.3 代表程序执行时效率
			G->pheromoneMatrix[i][j] += Matrix[i][j];//x(i,j) = ρx(i,j)+Δx(i,j)	
		}
	MatrixNormalization(G->pheromoneMatrix, G->num_ver, 1);//矩阵归一化
	return;
}

//更新数据库中信息素矩阵  目前还有问题，未调试
void updateMatrixDataBase(MYSQL* mysql, Graph*& G)
{
	//UPDATE `aco`.`pheromonematrix43` SET  `toVertex0` = 1 WHERE `ver_num` = 2
	string TableName = to_string(G->num_ver) + "pheromoneMatrix";
	string InsertStatement = "UPDATE `aco`.`" + TableName + "` SET  ";

	for (int i = 0; i < G->num_ver; i++)
	{
		for (int j = 0; j < G->num_ver; j++)
		{
			if (j != G->num_ver - 1) InsertStatement = InsertStatement + "`toVertex" + to_string(j) + "` = " + to_string(G->pheromoneMatrix[i][j]) + ",";
			else InsertStatement = InsertStatement + "`toVertex" + to_string(j) + "` = " \
				+ to_string(G->pheromoneMatrix[i][j]) + " WHERE `ver_num` = " + to_string(i) + ";";
		}
		if (mysql_query(mysql, InsertStatement.c_str())) cout << "Insert to antPath table record " + to_string(i) + " defeated!" << endl;
	}
}

int main()
{
	clock_t startTime, endTime,endTime1;
	
	int num_city;//总城市数
	int CarCapacity;//车容量
	string cityId[MaxPlace];//各城市ID
	int cityDemand[MaxPlace] = { 0 };//各城市需求
	int x_place[MaxPlace] = { 0 } , y_place[MaxPlace] = { 0 };//各城市坐标
	int sumDemand = 0, paths = 0;
	int DataBase = 0;

	cout << "请输入数据库类型 总顶点数 车容量大小："; 
	cin >> DataBase >> num_city >> CarCapacity;

	cout << "请依次输入各顶点id, x坐标，y坐标，需求：" << endl;
	for (int i = 0; i < num_city; i++)
	{
		cin >> cityId[i] >> x_place[i] >> y_place[i] >> cityDemand[i];
		sumDemand += cityDemand[i];
	}
	paths = sumDemand / CarCapacity + 1;

	double** Matrix;//邻接矩阵
	Matrix = new double* [num_city];
	for (int i = 0; i < num_city; i++)
		Matrix[i] = new double[num_city];

	for (int i = 0; i < num_city; i++)
		for (int j = i; j < num_city; j++)
			if (i == j)  Matrix[j][i] = Matrix[i][j] = 0;
			else Matrix[i][j] = Matrix[j][i] = sqrt(pow(x_place[i] - x_place[j], 2) + pow(y_place[i] - y_place[j], 2));
	
	MatrixNormalization(Matrix, num_city, 100);//矩阵归一化
	srand(time(0));

	Graph* MapCity = new Graph();
	CreateGraph(MapCity, num_city, cityId, Matrix, cityDemand, x_place, y_place);

	startTime = clock();//计时开始
	cout << "计时开始：开始连接数据库并插入表：" << endl;

	//数据库
	MYSQL mysql;
	mysql_init(&mysql);
	if (!ConnectSQL(&mysql,DataBase)) return -1;

	
	//创建数据库中的表格
	CreateTables(MapCity, &mysql,  paths);
	//插入城市数据
	InsertTablePlace(MapCity, &mysql);
	//插入矩阵信息
	InsertTableMatrix(MapCity, &mysql);

	endTime1 = clock();//计时结束
	cout << "至此数据库插入结束，费时:" << (double)(endTime1 - startTime) / CLOCKS_PER_SEC << "s" << endl;

	int num = 3000;//,即进行100轮
	string* BastPath = NULL;
	Ant* bestAntUntilNow = new Ant();
	bestAntUntilNow->sumLength = 1000000;
	bool is_change = false;
	while (num--)
	{
		Ant* ant[RoundAntNum];//总蚂蚁数2000*20
		Ant* bestAnt = new Ant();
		int bestSum = 10000;
		for (int i = 0; i < RoundAntNum; i++)
		{
			ant[i] = new Ant();
			ant[i]->passPath[ant[i]->count] = 0;//初始就在中心节点，即0号位置
			ant[i]->surplusCapacity = CarCapacity;
			ant[i]->count = 1;
			int count = 0;

			while (count++ != num_city - 1)
			{
				AntMove(MapCity, ant[i], CarCapacity);//蚂蚁移动
			}
			ant[i]->passPath[ant[i]->count++] = 0;
			ant[i]->sumLength += MapCity->adjacentMatrix[ant[i]->cityNumber][0];

			if (ant[i]->sumLength <= bestSum) {
				bestAnt = ant[i];
				bestSum = ant[i]->sumLength;
			}
		}

		if (bestAnt->sumLength < bestAntUntilNow->sumLength) {
			bestAntUntilNow = bestAnt;
			is_change = true;
		}

		if (num % RoundAntNum == 0)
		{
			updateMatrix(MapCity, ant, bestAntUntilNow);
			if (is_change) {
				string* path = AntPath(bestAntUntilNow, paths);
				InsertPath(&mysql, MapCity, bestAntUntilNow, 3000 - num, path, paths);
				is_change = false;
			}
		}
	}

	endTime = clock();//计时结束
	cout << "算法执行花费时间:" << (double)(endTime - endTime1) / CLOCKS_PER_SEC << "s" << endl;

	FreeConnect(&mysql);
	system("pause");
	return 0;
}