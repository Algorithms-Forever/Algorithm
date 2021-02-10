#pragma warning( disable : 4996)
#include <iostream>
#include <string>
#include <cmath>
#include <ctime>
#include <mysql.h>

constexpr int MaxPlace = 200;//��������
//constexpr int Q = 10;//��Ϣ����صĳ���ֵ  ������ã�����ȫ���ԣ��õ�ǰ���ŵ�·��������Ϊ��Ϣ����ر���
constexpr int RoundAntNum = 20;//ÿ��ʹ��������

using namespace std;

struct City//����
{
	string name;//����
	int x_place=0, y_place=0;//����
	int number=0;//���
	int demand = 0;//����
};

struct Graph //��ͼ �ڽӾ���洢
{
	double** adjacentMatrix = new double* [MaxPlace],      //�ڽӾ���
		** inspiringMatrix = new double* [MaxPlace],             //�������Ӿ���
		** pheromoneMatrix = new double* [MaxPlace];        //��Ϣ�ؾ���

	City *ver = new City[MaxPlace];//���㼯
	int num_ver = 0;//�������
};

struct Ant
{
	int cityNumber = 0;//��ǰ�������б��
	int* passPath = new int[2 * MaxPlace];//·���켣���澭���ĳ��б�ţ�������������ȫ�̸���Ϣ��ȷ��
	int surplusCapacity = 0;//ʣ������
	
	double sumLength=0;//�߹�����·������
	int count = 0;//��¼���߹��ĳ�����
};

//��������
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

//�ͷ�����
void FreeConnect(MYSQL *mysql)//�ͷ�����
{
	mysql_close(mysql);
}

//������
void CreateTables (Graph*& G, MYSQL *mysql, int paths)
{
	string ifExist = "DROP TABLE IF EXISTS `";

	//����place ��
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

	//�����ڽӾ�����Ϣ�ر�
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

	//�������� ÿ��������·����path
	string pathTableName = to_string(G->num_ver) + "path";
	string ifExistPath = ifExist + pathTableName + "`;";

	string createStatementPath = "CREATE TABLE `" + pathTableName + "`  (  `times` int(0) NOT NULL COMMENT '��¼����',";
	for (int i = 0; i < paths; i++)
		createStatementPath += "`path" + to_string(i) + "`  tinytext CHARACTER SET utf8 COLLATE utf8_general_ci NULL,";

	createStatementPath += "`length` int(0) NOT NULL,\
		PRIMARY KEY (`times`) USING BTREE\
		) ENGINE = InnoDB CHARACTER SET = utf8 COLLATE = utf8_general_ci ROW_FORMAT = Dynamic; ";

	mysql_query(mysql, ifExistPath.c_str());
	if(mysql_query(mysql, createStatementPath.c_str())) cout << "Create Table " + pathTableName + " defeated!" << endl;
}

//��������������
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

//������������Ϣ  �ڽӾ���  ��Ϣ�ؾ���
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

//�����һ�� Matrix�Ǿ��� length�Ǿ����С scope�Ǳ���
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

//����������Ϣ������ͼ
void CreateGraph(Graph*& G, int num_city, string* cityName, double** Matrix,int *demand, int* x_place, int* y_place)
{
	for (int i = 0; i < num_city; i++)//��ͼ�ĸ�������ϱ�ź͵�ַ,����
	{
		G->ver[i].name = cityName[i];
		G->ver[i].number = i;
		G->ver[i].x_place = x_place[i];
		G->ver[i].y_place = y_place[i];
		G->ver[i].demand = demand[i];
	}

	//ͼ�ĳ�ʼ��
	for (int i = 0; i < num_city; i++)
	{
		G->adjacentMatrix[i] = new double[num_city];
		G->inspiringMatrix[i] = new double[num_city];
		G->pheromoneMatrix[i] = new double[num_city];
	}

	//ͼ�ĸ�����ֵ
	for (int i = 0; i < num_city; i++)
		for (int j = i ; j < num_city; j++)
		{
			G->adjacentMatrix[j][i] = G->adjacentMatrix[i][j] = int(sqrt(pow(x_place[i] - x_place[j], 2) + pow(y_place[i] - y_place[j], 2)));//�ڽӾ���
			G->inspiringMatrix[j][i] = G->inspiringMatrix[i][j] = (Matrix[i][j] != 0 ? 1 / Matrix[i][j] : 0);//�������Ӿ���
			G->pheromoneMatrix[j][i] = G->pheromoneMatrix[i][j] = (i == j ? 0 : 1);//��Ϣ�ؾ���
		}
	G->num_ver = num_city;
}

//�ж�Ԫ��a�Ƿ��ڼ���array��
bool IsInSet(int a, int* array,int num)
{
	for (int i = 0; i < num; i++)
		if (array[i] == a) return true;
	return false;
}

//�����ƶ������������������㷨
void AntMove(Graph*& G,  Ant*& ant, int CarCapacity)
{
	//��ѡ����ʵ�·������״̬ת�ƹ���,�ݶ� a=1��b=1
	double* probability = new double[G->num_ver], sum = 0;
	int a = 1,b = 1;
	for (int i = 0; i < G->num_ver ; i++)
	{
		//�п�����һ����ȥ�ĵ㣬�Լ������Դ�С
		if (!IsInSet(i, ant->passPath, ant->count) && G->ver[i].demand <= ant->surplusCapacity)
			probability[i] = pow(G->pheromoneMatrix[ant->cityNumber][i], a) *
				pow(G->inspiringMatrix[ant->cityNumber][i], b);
		else probability[i] = 0;
		sum += probability[i];
	}
	
	if (sum == 0)//��û��һ�������Ҫ��
	{
		//���һ�����еĵ㶼�Ѿ��߹��ˣ�
		//���ʱ��Ӧ�������������ﷵ�أ��������ﲻ���������������

		//����������е�û�߹������ǵ�ǰ����ʣ������������
		ant->count++;//�߹���������һ
		ant->sumLength += G->adjacentMatrix[ant->cityNumber][0];//���㵱ǰ�ܳ���
		ant->passPath[ant->count-1] = 0;//д�뾭��·��
		ant->cityNumber = 0;//�޸ĵ�ǰλ��
		ant->surplusCapacity = CarCapacity;//�޸ĵ�ǰ����ʣ������
		AntMove(G, ant, CarCapacity);//��Ϊ��ǰ�ⲽ�����ˣ����ԣ��ⲽ���㣬����һ��
		return;
	}

	//�������ֵ 0 0.1 0.2 0.3 0.05
	double RandomNum = (rand() % 100 / double(100)) * sum;
	int choose = -1;//ѡ�нڵ�ı��

	for (choose; choose < G->num_ver && RandomNum>=0; )//ȷ��ѡ�еı��
		RandomNum -= probability[++choose];

	//ȷ����һ�����ĺ󣬶����ϵ����Խ����޸ģ��Լ�¼·��
	ant->count++;//�߹���������һ
	ant->sumLength += G->adjacentMatrix[ant->cityNumber][choose];//���㵱ǰ�ܳ���
	ant->passPath[ant->count-1] = choose;//д�뾭��·��
	ant->cityNumber = choose;//�޸ĵ�ǰλ��
	ant->surplusCapacity -= G->ver[choose].demand;//�޸�����ʣ������
	return;
}

//��������·��
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

//��������·���洢�����ݿ�
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

//������Ϣ�ؾ��� �����µĲ���������������
void updateMatrix(Graph*& G, Ant* ant[],Ant* bestAnt)
{
	double** Matrix;//����Ϣ�ؾ���
	Matrix = new double* [G->num_ver];
	for (int i = 0; i < G->num_ver; i++)//��ʼ��
	{
		Matrix[i] = new double[G->num_ver];
		for (int j = 0; j < G->num_ver; j++)
			Matrix[i][j] = 0;
	}

	for (int k = 0; k < RoundAntNum; k++)
	{
		double increment = bestAnt->sumLength / (ant[k]->sumLength * RoundAntNum);
		for (int i = 0; i < ant[k]->count - 2; i++)//�������
		{
			Matrix[ant[k]->passPath[i]][ant[k]->passPath[i + 1]] += increment;
		}
		Matrix[ant[k]->passPath[ant[k]->count-2]][ant[k]->passPath[0]] += increment;
	}

	for (int i = 0; i < G->num_ver; i++)
		for (int j = 0; j < G->num_ver; j++) 
		{
			G->pheromoneMatrix[i][j] *= 0.7;//��������Ϊ0.3 �������ִ��ʱЧ��
			G->pheromoneMatrix[i][j] += Matrix[i][j];//x(i,j) = ��x(i,j)+��x(i,j)	
		}
	MatrixNormalization(G->pheromoneMatrix, G->num_ver, 1);//�����һ��
	return;
}

//�������ݿ�����Ϣ�ؾ���  Ŀǰ�������⣬δ����
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
	
	int num_city;//�ܳ�����
	int CarCapacity;//������
	string cityId[MaxPlace];//������ID
	int cityDemand[MaxPlace] = { 0 };//����������
	int x_place[MaxPlace] = { 0 } , y_place[MaxPlace] = { 0 };//����������
	int sumDemand = 0, paths = 0;
	int DataBase = 0;

	cout << "���������ݿ����� �ܶ����� ��������С��"; 
	cin >> DataBase >> num_city >> CarCapacity;

	cout << "���������������id, x���꣬y���꣬����" << endl;
	for (int i = 0; i < num_city; i++)
	{
		cin >> cityId[i] >> x_place[i] >> y_place[i] >> cityDemand[i];
		sumDemand += cityDemand[i];
	}
	paths = sumDemand / CarCapacity + 1;

	double** Matrix;//�ڽӾ���
	Matrix = new double* [num_city];
	for (int i = 0; i < num_city; i++)
		Matrix[i] = new double[num_city];

	for (int i = 0; i < num_city; i++)
		for (int j = i; j < num_city; j++)
			if (i == j)  Matrix[j][i] = Matrix[i][j] = 0;
			else Matrix[i][j] = Matrix[j][i] = sqrt(pow(x_place[i] - x_place[j], 2) + pow(y_place[i] - y_place[j], 2));
	
	MatrixNormalization(Matrix, num_city, 100);//�����һ��
	srand(time(0));

	Graph* MapCity = new Graph();
	CreateGraph(MapCity, num_city, cityId, Matrix, cityDemand, x_place, y_place);

	startTime = clock();//��ʱ��ʼ
	cout << "��ʱ��ʼ����ʼ�������ݿⲢ�����" << endl;

	//���ݿ�
	MYSQL mysql;
	mysql_init(&mysql);
	if (!ConnectSQL(&mysql,DataBase)) return -1;

	
	//�������ݿ��еı��
	CreateTables(MapCity, &mysql,  paths);
	//�����������
	InsertTablePlace(MapCity, &mysql);
	//���������Ϣ
	InsertTableMatrix(MapCity, &mysql);

	endTime1 = clock();//��ʱ����
	cout << "�������ݿ�����������ʱ:" << (double)(endTime1 - startTime) / CLOCKS_PER_SEC << "s" << endl;

	int num = 3000;//,������100��
	string* BastPath = NULL;
	Ant* bestAntUntilNow = new Ant();
	bestAntUntilNow->sumLength = 1000000;
	bool is_change = false;
	while (num--)
	{
		Ant* ant[RoundAntNum];//��������2000*20
		Ant* bestAnt = new Ant();
		int bestSum = 10000;
		for (int i = 0; i < RoundAntNum; i++)
		{
			ant[i] = new Ant();
			ant[i]->passPath[ant[i]->count] = 0;//��ʼ�������Ľڵ㣬��0��λ��
			ant[i]->surplusCapacity = CarCapacity;
			ant[i]->count = 1;
			int count = 0;

			while (count++ != num_city - 1)
			{
				AntMove(MapCity, ant[i], CarCapacity);//�����ƶ�
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

	endTime = clock();//��ʱ����
	cout << "�㷨ִ�л���ʱ��:" << (double)(endTime - endTime1) / CLOCKS_PER_SEC << "s" << endl;

	FreeConnect(&mysql);
	system("pause");
	return 0;
}