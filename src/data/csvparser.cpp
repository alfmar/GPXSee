#include <QByteArrayList>
#include "common/csv.h"
#include "csvparser.h"

bool CSVParser::parse(QFile *file, QList<TrackData> &tracks,
  QList<RouteData> &routes, QList<Area> &polygons,
  QVector<Waypoint> &waypoints)
{
	Q_UNUSED(tracks);
	Q_UNUSED(routes);
	Q_UNUSED(polygons);
	CSV csv(file);
	QByteArrayList entry;
	bool ok;

	while (!csv.atEnd()) {
		if (!csv.readEntry(entry) || entry.size() < 3) {
			_errorString = "Parse error";
			_errorLine = csv.line();
			return false;
		}

		double lon = entry.at(0).toDouble(&ok);
		if (!ok || (lon < -180.0 || lon > 180.0)) {
			_errorString = "Invalid longitude";
			_errorLine = csv.line();
			return false;
		}
		double lat = entry.at(1).toDouble(&ok);
		if (!ok || (lat < -90.0 || lat > 90.0)) {
			_errorString = "Invalid latitude";
			_errorLine = csv.line();
			return false;
		}
		Waypoint wp(Coordinates(lon, lat));
		wp.setName(entry.at(2));
		if (entry.size() > 3)
			wp.setDescription(entry.at(3));

		waypoints.append(wp);
	}

	return true;
}
