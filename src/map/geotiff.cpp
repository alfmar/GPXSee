#include <QFile>
#include "common/tifffile.h"
#include "pcs.h"
#include "geotiff.h"


#define ModelPixelScaleTag             33550
#define ModelTiepointTag               33922
#define ModelTransformationTag         34264
#define GeoKeyDirectoryTag             34735
#define GeoDoubleParamsTag             34736

#define GTModelTypeGeoKey              1024
#define GTRasterTypeGeoKey             1025
#define GeographicTypeGeoKey           2048
#define GeogGeodeticDatumGeoKey        2050
#define GeogPrimeMeridianGeoKey        2051
#define GeogAngularUnitsGeoKey         2054
#define GeogEllipsoidGeoKey            2056
#define GeogAzimuthUnitsGeoKey         2060
#define ProjectedCSTypeGeoKey          3072
#define ProjectionGeoKey               3074
#define ProjCoordTransGeoKey           3075
#define ProjLinearUnitsGeoKey          3076
#define ProjStdParallel1GeoKey         3078
#define ProjStdParallel2GeoKey         3079
#define ProjNatOriginLongGeoKey        3080
#define ProjNatOriginLatGeoKey         3081
#define ProjFalseEastingGeoKey         3082
#define ProjFalseNorthingGeoKey        3083
#define ProjFalseOriginLongGeoKey      3084
#define ProjFalseOriginLatGeoKey       3085
#define ProjFalseOriginEastingGeoKey   3086
#define ProjFalseOriginNorthingGeoKey  3087
#define ProjCenterLongGeoKey           3088
#define ProjCenterLatGeoKey            3089
#define ProjCenterEastingGeoKey        3090
#define ProjCenterNorthingGeoKey       3091
#define ProjScaleAtNatOriginGeoKey     3092
#define ProjScaleAtCenterGeoKey        3093
#define ProjAzimuthAngleGeoKey         3094
#define ProjStraightVertPoleLongGeoKey 3095
#define ProjRectifiedGridAngleGeoKey   3096

#define ModelTypeProjected             1
#define ModelTypeGeographic            2
#define ModelTypeGeocentric            3

#define CT_TransverseMercator          1
#define CT_ObliqueMercator             3
#define CT_Mercator                    7
#define CT_LambertConfConic_2SP        8
#define CT_LambertConfConic_1SP        9
#define CT_LambertAzimEqualArea        10
#define CT_AlbersEqualArea             11
#define CT_PolarStereographic          15
#define CT_ObliqueStereographic        16
#define CT_Polyconic                   22


#define IS_SET(map, key) \
	((map).contains(key) && (map).value(key).SHORT != 32767)


struct GeoKeyHeader {
	quint16 KeyDirectoryVersion;
	quint16 KeyRevision;
	quint16 MinorRevision;
	quint16 NumberOfKeys;
};

struct GeoKeyEntry {
	quint16 KeyID;
	quint16 TIFFTagLocation;
	quint16 Count;
	quint16 ValueOffset;
};


bool GeoTIFF::readEntry(TIFFFile &file, Ctx &ctx) const
{
	quint16 tag;
	quint16 type;
	quint32 count, offset;

	if (!file.readValue(tag))
		return false;
	if (!file.readValue(type))
		return false;
	if (!file.readValue(count))
		return false;
	if (!file.readValue(offset))
		return false;

	switch (tag) {
		case ModelPixelScaleTag:
			if (type != TIFF_DOUBLE || count != 3)
				return false;
			ctx.scale = offset;
			break;
		case ModelTiepointTag:
			if (type != TIFF_DOUBLE || count < 6 || count % 6)
				return false;
			ctx.tiepoints = offset;
			ctx.tiepointCount = count / 6;
			break;
		case GeoKeyDirectoryTag:
			if (type != TIFF_SHORT)
				return false;
			ctx.keys = offset;
			break;
		case GeoDoubleParamsTag:
			if (type != TIFF_DOUBLE)
				return false;
			ctx.values = offset;
			break;
		case ModelTransformationTag:
			if (type != TIFF_DOUBLE || count != 16)
				return false;
			ctx.matrix = offset;
			break;
	}

	return true;
}

bool GeoTIFF::readIFD(TIFFFile &file, quint32 offset, Ctx &ctx) const
{
	quint16 count;

	if (!file.seek(offset))
		return false;
	if (!file.readValue(count))
		return false;

	for (quint16 i = 0; i < count; i++)
		if (!readEntry(file, ctx))
			return false;

	return true;
}

bool GeoTIFF::readScale(TIFFFile &file, quint32 offset, PointD &scale) const
{
	if (!file.seek(offset))
		return false;

	if (!file.readValue(scale.rx()))
		return false;
	if (!file.readValue(scale.ry()))
		return false;

	return true;
}

bool GeoTIFF::readTiepoints(TIFFFile &file, quint32 offset, quint32 count,
  QList<ReferencePoint> &points) const
{
	double z, pz;
	PointD xy, pp;

	if (!file.seek(offset))
		return false;

	for (quint32 i = 0; i < count; i++) {
		if (!file.readValue(xy.rx()))
			return false;
		if (!file.readValue(xy.ry()))
			return false;
		if (!file.readValue(z))
			return false;

		if (!file.readValue(pp.rx()))
			return false;
		if (!file.readValue(pp.ry()))
			return false;
		if (!file.readValue(pz))
			return false;

		points.append(ReferencePoint(xy, pp));
	}

	return true;
}

bool GeoTIFF::readMatrix(TIFFFile &file, quint32 offset, double matrix[16]) const
{
	if (!file.seek(offset))
		return false;

	for (int i = 0; i < 16; i++)
		if (!file.readValue(matrix[i]))
			return false;

	return true;
}

bool GeoTIFF::readKeys(TIFFFile &file, Ctx &ctx, QMap<quint16, Value> &kv) const
{
	GeoKeyHeader header;
	GeoKeyEntry entry;
	Value value;

	if (!file.seek(ctx.keys))
		return false;

	if (!file.readValue(header.KeyDirectoryVersion))
		return false;
	if (!file.readValue(header.KeyRevision))
		return false;
	if (!file.readValue(header.MinorRevision))
		return false;
	if (!file.readValue(header.NumberOfKeys))
		return false;

	for (int i = 0; i < header.NumberOfKeys; i++) {
		if (!file.readValue(entry.KeyID))
			return false;
		if (!file.readValue(entry.TIFFTagLocation))
			return false;
		if (!file.readValue(entry.Count))
			return false;
		if (!file.readValue(entry.ValueOffset))
			return false;

		switch (entry.KeyID) {
			case GTModelTypeGeoKey:
			case GTRasterTypeGeoKey:
			case GeographicTypeGeoKey:
			case GeogGeodeticDatumGeoKey:
			case GeogPrimeMeridianGeoKey:
			case GeogAngularUnitsGeoKey:
			case GeogAzimuthUnitsGeoKey:
			case ProjectedCSTypeGeoKey:
			case ProjectionGeoKey:
			case ProjCoordTransGeoKey:
			case ProjLinearUnitsGeoKey:
			case GeogEllipsoidGeoKey:
				if (entry.TIFFTagLocation != 0 || entry.Count != 1)
					return false;
				value.SHORT = entry.ValueOffset;
				kv.insert(entry.KeyID, value);
				break;
			case ProjScaleAtNatOriginGeoKey:
			case ProjNatOriginLongGeoKey:
			case ProjNatOriginLatGeoKey:
			case ProjFalseEastingGeoKey:
			case ProjFalseNorthingGeoKey:
			case ProjStdParallel1GeoKey:
			case ProjStdParallel2GeoKey:
			case ProjCenterLongGeoKey:
			case ProjCenterLatGeoKey:
			case ProjScaleAtCenterGeoKey:
			case ProjAzimuthAngleGeoKey:
			case ProjRectifiedGridAngleGeoKey:
			case ProjFalseOriginLongGeoKey:
			case ProjFalseOriginLatGeoKey:
			case ProjCenterEastingGeoKey:
			case ProjCenterNorthingGeoKey:
			case ProjFalseOriginEastingGeoKey:
			case ProjFalseOriginNorthingGeoKey:
			case ProjStraightVertPoleLongGeoKey:
				if (!readGeoValue(file, ctx.values, entry.ValueOffset,
				  value.DOUBLE))
					return false;
				kv.insert(entry.KeyID, value);
				break;
			default:
				break;
		}
	}

	return true;
}

bool GeoTIFF::readGeoValue(TIFFFile &file, quint32 offset, quint16 index,
  double &val) const
{
	qint64 pos = file.pos();

	if (!file.seek(offset + index * sizeof(double)))
		return false;
	if (!file.readValue(val))
		return false;

	if (!file.seek(pos))
		return false;

	return true;
}

GCS GeoTIFF::geographicCS(QMap<quint16, Value> &kv)
{
	GCS gcs;

	if (IS_SET(kv, GeographicTypeGeoKey)) {
		gcs = GCS::gcs(kv.value(GeographicTypeGeoKey).SHORT);
		if (gcs.isNull())
			_errorString = QString("%1: unknown GCS")
			  .arg(kv.value(GeographicTypeGeoKey).SHORT);
	} else if (IS_SET(kv, GeogGeodeticDatumGeoKey)
	  || kv.value(GeogEllipsoidGeoKey).SHORT == 7019
	  || kv.value(GeogEllipsoidGeoKey).SHORT == 7030) {
		int pm = IS_SET(kv, GeogPrimeMeridianGeoKey)
		  ? kv.value(GeogPrimeMeridianGeoKey).SHORT : 8901;
		int au = IS_SET(kv, GeogAngularUnitsGeoKey)
		  ? kv.value(GeogAngularUnitsGeoKey).SHORT : 9102;

		// If only the ellipsoid is defined and it is GRS80 or WGS84, handle
		// such definition as a WGS84 geodetic datum.
		int gd = IS_SET(kv, GeogGeodeticDatumGeoKey)
		  ? kv.value(GeogGeodeticDatumGeoKey).SHORT : 6326;

		gcs = GCS::gcs(gd, pm, au);
		if (gcs.isNull())
			_errorString = QString("%1+%2+%3: unknown geodetic datum + prime"
			  " meridian + units combination").arg(gd).arg(pm).arg(au);
	} else
		_errorString = "Can not determine GCS";

	return gcs;
}

Projection::Method GeoTIFF::coordinateTransformation(QMap<quint16, Value> &kv)
{
	if (!IS_SET(kv, ProjCoordTransGeoKey)) {
		_errorString = "Missing coordinate transformation method";
		return Projection::Method();
	}

	switch (kv.value(ProjCoordTransGeoKey).SHORT) {
		case CT_TransverseMercator:
			return Projection::Method(9807);
		case CT_ObliqueMercator:
			return Projection::Method(9815);
		case CT_Mercator:
			return Projection::Method(9804);
		case CT_LambertConfConic_2SP:
			return Projection::Method(9802);
		case CT_LambertConfConic_1SP:
			return Projection::Method(9801);
		case CT_LambertAzimEqualArea:
			return Projection::Method(9820);
		case CT_AlbersEqualArea:
			return Projection::Method(9822);
		case CT_PolarStereographic:
			return Projection::Method(9829);
		case CT_ObliqueStereographic:
			return Projection::Method(9809);
		case CT_Polyconic:
			return Projection::Method(9818);
		default:
			_errorString = QString("%1: unknown coordinate transformation method")
			  .arg(kv.value(ProjCoordTransGeoKey).SHORT);
			return Projection::Method();
	}
}

bool GeoTIFF::projectedModel(QMap<quint16, Value> &kv)
{
	if (IS_SET(kv, ProjectedCSTypeGeoKey)) {
		PCS pcs(PCS::pcs(kv.value(ProjectedCSTypeGeoKey).SHORT));
		if (pcs.isNull()) {
			_errorString = QString("%1: unknown PCS")
			  .arg(kv.value(ProjectedCSTypeGeoKey).SHORT);
			return false;
		}
		_projection = Projection(pcs);
	} else if (IS_SET(kv, ProjectionGeoKey)) {
		GCS gcs(geographicCS(kv));
		if (gcs.isNull())
			return false;
		Conversion c(Conversion::conversion(kv.value(ProjectionGeoKey).SHORT));
		if (c.isNull()) {
			_errorString = QString("%1: unknown projection code")
			  .arg(kv.value(ProjectionGeoKey).SHORT);
			return false;
		}
		_projection = Projection(PCS(gcs, c));
	} else {
		double lat0, lon0, scale, fe, fn, sp1, sp2;

		GCS gcs(geographicCS(kv));
		if (gcs.isNull())
			return false;
		Projection::Method method(coordinateTransformation(kv));
		if (method.isNull())
			return false;

		AngularUnits au(IS_SET(kv, GeogAngularUnitsGeoKey)
		  ? kv.value(GeogAngularUnitsGeoKey).SHORT : 9102);
		AngularUnits zu(IS_SET(kv, GeogAzimuthUnitsGeoKey)
		  ? kv.value(GeogAzimuthUnitsGeoKey).SHORT : 9102);
		LinearUnits lu(IS_SET(kv, ProjLinearUnitsGeoKey)
		  ? kv.value(ProjLinearUnitsGeoKey).SHORT : 9001);
		if (lu.isNull()) {
			_errorString = QString("%1: unknown projection linear units code")
			  .arg(kv.value(ProjLinearUnitsGeoKey).SHORT);
			return false;
		}

		if (kv.contains(ProjNatOriginLatGeoKey))
			lat0 = au.toDegrees(kv.value(ProjNatOriginLatGeoKey).DOUBLE);
		else if (kv.contains(ProjCenterLatGeoKey))
			lat0 = au.toDegrees(kv.value(ProjCenterLatGeoKey).DOUBLE);
		else if (kv.contains(ProjFalseOriginLatGeoKey))
			lat0 = au.toDegrees(kv.value(ProjFalseOriginLatGeoKey).DOUBLE);
		else
			lat0 = NAN;
		if (kv.contains(ProjNatOriginLongGeoKey))
			lon0 = au.toDegrees(kv.value(ProjNatOriginLongGeoKey).DOUBLE);
		else if (kv.contains(ProjCenterLongGeoKey))
			lon0 = au.toDegrees(kv.value(ProjCenterLongGeoKey).DOUBLE);
		else if (kv.contains(ProjFalseOriginLongGeoKey))
			lon0 = au.toDegrees(kv.value(ProjFalseOriginLongGeoKey).DOUBLE);
		else if (kv.contains(ProjStraightVertPoleLongGeoKey))
			lon0 = au.toDegrees(kv.value(ProjStraightVertPoleLongGeoKey).DOUBLE);
		else
			lon0 = NAN;
		if (kv.contains(ProjScaleAtNatOriginGeoKey))
			scale = kv.value(ProjScaleAtNatOriginGeoKey).DOUBLE;
		else if (kv.contains(ProjScaleAtCenterGeoKey))
			scale = kv.value(ProjScaleAtCenterGeoKey).DOUBLE;
		else
			scale = NAN;
		if (kv.contains(ProjStdParallel1GeoKey))
			sp1 = au.toDegrees(kv.value(ProjStdParallel1GeoKey).DOUBLE);
		else if (kv.contains(ProjAzimuthAngleGeoKey))
			sp1 = zu.toDegrees(kv.value(ProjAzimuthAngleGeoKey).DOUBLE);
		else
			sp1 = NAN;
		if (kv.contains(ProjStdParallel2GeoKey))
			sp2 = au.toDegrees(kv.value(ProjStdParallel2GeoKey).DOUBLE);
		else if (kv.contains(ProjRectifiedGridAngleGeoKey))
			sp2 = au.toDegrees(kv.value(ProjRectifiedGridAngleGeoKey).DOUBLE);
		else
			sp2 = NAN;
		if (kv.contains(ProjFalseNorthingGeoKey))
			fn = lu.toMeters(kv.value(ProjFalseNorthingGeoKey).DOUBLE);
		else if (kv.contains(ProjCenterNorthingGeoKey))
			fn = lu.toMeters(kv.value(ProjCenterNorthingGeoKey).DOUBLE);
		else if (kv.contains(ProjFalseOriginNorthingGeoKey))
			fn = lu.toMeters(kv.value(ProjFalseOriginNorthingGeoKey).DOUBLE);
		else
			fn = NAN;
		if (kv.contains(ProjFalseEastingGeoKey))
			fe = lu.toMeters(kv.value(ProjFalseEastingGeoKey).DOUBLE);
		else if (kv.contains(ProjCenterEastingGeoKey))
			fe = lu.toMeters(kv.value(ProjCenterEastingGeoKey).DOUBLE);
		else if (kv.contains(ProjFalseOriginEastingGeoKey))
			fe = lu.toMeters(kv.value(ProjFalseOriginEastingGeoKey).DOUBLE);
		else
			fe = NAN;

		Projection::Setup setup(lat0, lon0, scale, fe, fn, sp1, sp2);
		_projection = Projection(PCS(gcs, Conversion(method, setup, lu)));
	}

	return true;
}

bool GeoTIFF::geographicModel(QMap<quint16, Value> &kv)
{
	GCS gcs(geographicCS(kv));
	if (gcs.isNull())
		return false;

	_projection = Projection(gcs);

	return true;
}

GeoTIFF::GeoTIFF(const QString &path)
{
	QList<ReferencePoint> points;
	PointD scale;
	QMap<quint16, Value> kv;
	Ctx ctx;


	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		_errorString = file.errorString();
		return;
	}

	TIFFFile tiff(&file);
	if (!tiff.isValid()) {
		_errorString = "Not a TIFF file";
		return;
	}

	for (quint32 ifd = tiff.ifd(); ifd; ) {
		if (!readIFD(tiff, ifd, ctx) || !tiff.readValue(ifd)) {
			_errorString = "Invalid IFD";
			return;
		}
	}

	if (!ctx.keys) {
		_errorString = "Not a GeoTIFF file";
		return;
	}

	if (ctx.scale) {
		if (!readScale(tiff, ctx.scale, scale)) {
			_errorString = "Error reading model pixel scale";
			return;
		}
	}
	if (ctx.tiepoints) {
		if (!readTiepoints(tiff, ctx.tiepoints, ctx.tiepointCount, points)) {
			_errorString = "Error reading raster->model tiepoint pairs";
			return;
		}
	}

	if (!readKeys(tiff, ctx, kv)) {
		_errorString = "Error reading Geo key/value";
		return;
	}

	switch (kv.value(GTModelTypeGeoKey).SHORT) {
		case ModelTypeProjected:
			if (!projectedModel(kv))
				return;
			break;
		case ModelTypeGeographic:
			if (!geographicModel(kv))
				return;
			break;
		case ModelTypeGeocentric:
			_errorString = "Geocentric models are not supported";
			return;
		default:
			_errorString = "Unknown/missing model type";
			return;
	}

	if (ctx.scale && ctx.tiepoints)
		_transform = Transform(points.first(), scale);
	else if (ctx.tiepointCount > 1)
		_transform = Transform(points);
	else if (ctx.matrix) {
		double matrix[16];
		if (!readMatrix(tiff, ctx.matrix, matrix)) {
			_errorString = "Error reading transformation matrix";
			return;
		}
		_transform = Transform(matrix);
	} else {
		_errorString = "Incomplete/missing model transformation info";
		return;
	}

	if (!_transform.isValid())
		_errorString = _transform.errorString();
}
