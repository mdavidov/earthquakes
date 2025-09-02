#include "geojson_parser.hpp"

#include <QGeoCoordinate>
#include <QTest>

// Declare the test class
class TestGeoCoordinateParsing : public QObject {
    Q_OBJECT
private slots:
    void testParseCoordinate_data();
    void testParseCoordinate();
    void testParseDMSCoordinate_data();
    void testParseDMSCoordinate();
};

void TestGeoCoordinateParsing::testParseCoordinate_data() {
    QTest::addColumn<QString>("input");
    QTest::addColumn<double>("expectedLat");
    QTest::addColumn<double>("expectedLon");

    QTest::newRow("basic") << "34.0522,-118.2437" << 34.0522 << -118.2437;
    QTest::newRow("with spaces") << " 34.0522 , -118.2437 " << 34.0522 << -118.2437;
    QTest::newRow("invalid") << "abc,xyz" << 0.0 << 0.0;
}

void TestGeoCoordinateParsing::testParseCoordinate() {
    QFETCH(QString, input);
    QFETCH(double, expectedLat);
    QFETCH(double, expectedLon);

    QGeoCoordinate coord = GeoJsonParser::parseCoordinate(input);
    if (coord.isValid()) {
        QCOMPARE(coord.latitude(), expectedLat);
        QCOMPARE(coord.longitude(), expectedLon);
    } else {
        QVERIFY(input == "abc,xyz"); // Only invalid case
    }
}

void TestGeoCoordinateParsing::testParseDMSCoordinate_data() {
    QTest::addColumn<QString>("input");
    QTest::addColumn<double>("expectedLat");
    QTest::addColumn<double>("expectedLon");

    QTest::newRow("Sydney") << "34°03'08\"S, 151°12'34\"E" << -34.0522 << 151.2094;
    QTest::newRow("Los Angeles") << "34°03'08\"N, 118°14'37\"W" << 34.0522 << -118.2436;
    QTest::newRow("invalid") << "bad input" << 0.0 << 0.0;
}

void TestGeoCoordinateParsing::testParseDMSCoordinate() {
    QFETCH(QString, input);
    QFETCH(double, expectedLat);
    QFETCH(double, expectedLon);

    QGeoCoordinate coord = GeoJsonParser::parseDMSCoordinate(input);
    if (coord.isValid()) {
        QVERIFY(qAbs(coord.latitude() - expectedLat) < 0.001);
        QVERIFY(qAbs(coord.longitude() - expectedLon) < 0.001);
    } else {
        QVERIFY(input == "bad input");
    }
}

QTEST_MAIN(TestGeoCoordinateParsing)
#include "testgeocoordinateparsing.moc"
