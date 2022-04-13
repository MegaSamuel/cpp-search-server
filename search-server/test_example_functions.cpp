#include <cmath>

#include "test_example_functions.h"
#include "search_server.h"

using namespace std;

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, 
                     const std::string& file, const std::string& func, unsigned line, const std::string& hint)
{
    if(t != u) 
    {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;

        if(!hint.empty()) 
        {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;

        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& str_value, 
                const std::string& file, const std::string& func, unsigned line, const std::string& hint) 
{
    if(!value)
    {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << str_value << ") failed."s;

        if(!hint.empty())
        {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;

        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

//-----------------------------------------------------------------------------

template <typename Func>
void RunTestImpl(Func func, const std::string& str_func) 
{
    cerr << str_func << " "s;
    func();
    cerr << "OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

//-----------------------------------------------------------------------------

// -------- Начало модульных тестов поисковой системы ----------

// Добавленный документ должен находиться по поисковому запросу, который
// содержит слова из документа
void TestAddDocument()
{
    SearchServer server;

    {
        // на сервере ничего нет
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(0U, found_docs.size());
        ASSERT_EQUAL(0, server.GetDocumentCount());
    }

    {
        const int doc_id = 42;
        const string content = "cat in the city"s;
        const vector<int> ratings = {1, 2, 3};

        // добавили документ
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(1U, found_docs.size());
        ASSERT_EQUAL(1, server.GetDocumentCount());
        const Document& doc0 = found_docs[0];

        // проверяем
        ASSERT_EQUAL(doc_id, doc0.id);
    }

    {
        const int doc_id = 24;
        const string content = "dog in the town"s;
        const vector<int> ratings = {4, 5, 6};

        // добавили документ
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("dog"s);
        ASSERT_EQUAL(1U, found_docs.size());
        ASSERT_EQUAL(2, server.GetDocumentCount());
        const Document& doc0 = found_docs[0];

        // проверяем
        ASSERT_EQUAL(doc_id, doc0.id);
    }

    {
        const auto found_docs = server.FindTopDocuments("in the"s);
        ASSERT_EQUAL(2U, found_docs.size());
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];

        // проверяем
        ASSERT_EQUAL(24, doc0.id);

        ASSERT_EQUAL(42, doc1.id);
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(1U, found_docs.size());
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

// Документы, содержащие минус-слова поискового запроса, не должны включаться в 
// результаты поиска
void TestExcludeMinusWordsFromAddedDocumentContent()
{
    SearchServer server;

    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(24, "dog in the town"s, DocumentStatus::ACTUAL, {4, 5, 6});
    server.AddDocument(11, "is city a small town"s, DocumentStatus::ACTUAL, {3, 4, 5});
    server.AddDocument(22, "dog bark at the cat"s, DocumentStatus::ACTUAL, {1, 3, 5});

    const auto found_docs = server.FindTopDocuments("cat -dog"s);
    ASSERT_EQUAL(1U, found_docs.size());
    const Document& doc0 = found_docs[0];

    // проверяем
    ASSERT_EQUAL(42, doc0.id);
}

// При матчинге документа по поисковому запросу должны быть возвращены все слова
// из поискового запроса, присутствующие в документе
void TestDocumentMatch()
{
    SearchServer server("the"s);

    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(24, "dog in the town"s, DocumentStatus::ACTUAL, {4, 5, 6});
    server.AddDocument(11, "is city a small town"s, DocumentStatus::BANNED, {3, 4, 5});
    server.AddDocument(22, "dog bark at the cat"s, DocumentStatus::ACTUAL, {1, 3, 5});

    auto [vct1, stat1] = server.MatchDocument("cat city", 42);
    ASSERT_EQUAL(2U, vct1.size());
    ASSERT(DocumentStatus::ACTUAL == stat1);

    auto [vct2, stat2] = server.MatchDocument("town city", 11);
    ASSERT_EQUAL(2U, vct2.size());
    ASSERT(DocumentStatus::BANNED == stat2);

    auto [vct3, stat3] = server.MatchDocument("cat dog", 22);
    ASSERT_EQUAL(2U, vct3.size());
    ASSERT(DocumentStatus::ACTUAL == stat3);

    auto [vct4, stat4] = server.MatchDocument("in -dog", 24);
    ASSERT_EQUAL(0U, vct4.size());
    ASSERT(DocumentStatus::ACTUAL == stat4);
}

//  Возвращаемые при поиске документов результаты должны быть отсортированы в
// порядке убывания релевантности
void TestSortByRelavant()
{
    SearchServer server;

    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(24, "dog in the town"s, DocumentStatus::ACTUAL, {4, 5, 6});
    server.AddDocument(11, "is city a small town"s, DocumentStatus::ACTUAL, {3, 4, 5});
    server.AddDocument(22, "dog bark at the cat"s, DocumentStatus::ACTUAL, {1, 3, 5});

    const auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(2U, found_docs.size());
    const Document& doc0 = found_docs[0];
    const Document& doc1 = found_docs[1];

    // проверяем
    ASSERT_EQUAL(42, doc0.id);

    ASSERT_EQUAL(22, doc1.id);
}

// Рейтинг добавленного документа равен среднему арифметическому оценок документа
void TestCalcRating()
{
    SearchServer server;

    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(24, "dog in the town"s, DocumentStatus::ACTUAL, {-1, -2, -3});

    const auto found_docs = server.FindTopDocuments("in the"s);
    ASSERT_EQUAL(2U, found_docs.size());
    const Document& doc0 = found_docs[0];
    const Document& doc1 = found_docs[1];

    // проверяем
    ASSERT_EQUAL(42, doc0.id);
    ASSERT_EQUAL(2, doc0.rating);

    ASSERT_EQUAL(24, doc1.id);
    ASSERT_EQUAL(-2, doc1.rating);
}

// Фильтрация результатов поиска с использованием предиката, задаваемого пользователем
void TestFilterByPredicate()
{

}

// Поиск документов, имеющих заданный статус
void TestSearchByStatus()
{
    SearchServer server;

    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(24, "dog in the town"s, DocumentStatus::ACTUAL, {4, 5, 6});
    server.AddDocument(11, "is city a small town"s, DocumentStatus::IRRELEVANT, {3, 4, 5});
    server.AddDocument(22, "dog bark at the cat"s, DocumentStatus::BANNED, {1, 3, 5});
    server.AddDocument(33, "to be or not to be"s, DocumentStatus::REMOVED, {1, 3, 5});

    {
        const auto found_docs1 = server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(1U, found_docs1.size());
        const Document& doc0 = found_docs1[0];

        // проверяем
        ASSERT_EQUAL(42, doc0.id);

        const auto found_docs2 = server.FindTopDocuments("horse"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(0U, found_docs2.size());
    }

    {
        const auto found_docs1 = server.FindTopDocuments("small town"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(1U, found_docs1.size());
        const Document& doc0 = found_docs1[0];

        // проверяем
        ASSERT_EQUAL(11, doc0.id);

        const auto found_docs2 = server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(0U, found_docs2.size());
    }

    {
        const auto found_docs1 = server.FindTopDocuments("dog"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(1U, found_docs1.size());
        const Document& doc0 = found_docs1[0];

        // проверяем
        ASSERT_EQUAL(22, doc0.id);

        const auto found_docs2 = server.FindTopDocuments("small town"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(0U, found_docs2.size());
    }

    {
        const auto found_docs1 = server.FindTopDocuments("to be"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(1U, found_docs1.size());
        const Document& doc0 = found_docs1[0];

        // проверяем
        ASSERT_EQUAL(33, doc0.id);

        const auto found_docs2 = server.FindTopDocuments("fat cat"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(0U, found_docs2.size());
    }
}

// Корректное вычисление релевантности найденных документов
void TestCalcRelevant()
{
    SearchServer server;

    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(24, "dog in the town"s, DocumentStatus::ACTUAL, {4, 5, 6});
    server.AddDocument(11, "is city a small town"s, DocumentStatus::ACTUAL, {3, 4, 5});
    server.AddDocument(22, "dog bark at the cat"s, DocumentStatus::ACTUAL, {1, 3, 5});

    const auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(2U, found_docs.size());
    const Document& doc0 = found_docs[0];
    const Document& doc1 = found_docs[1];

    // проверяем
    ASSERT_EQUAL(42, doc0.id);
    ASSERT(doc0.relevance > 0.0);
    ASSERT(0.001 > fabs(doc0.relevance - 0.173287));

    ASSERT_EQUAL(22, doc1.id);
    ASSERT(doc1.relevance > 0.0);
    ASSERT(0.001 > fabs(doc1.relevance - 0.138629));
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestAddDocument);                               // добавление документов
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);  // учет стоп-слов
    RUN_TEST(TestExcludeMinusWordsFromAddedDocumentContent); // учет минус-слов
    RUN_TEST(TestDocumentMatch);                             // матчинг документов
    RUN_TEST(TestSortByRelavant);                            // сортировка результата по релевантности
    RUN_TEST(TestCalcRating);                                // вычисление рейтинга
    RUN_TEST(TestFilterByPredicate);                         // фильтрация по предикату
    RUN_TEST(TestSearchByStatus);                            // поиск документов по статусу
    RUN_TEST(TestCalcRelevant);                              // вычисление релевантности
}
