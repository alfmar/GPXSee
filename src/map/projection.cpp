#include "proj/mercator.h"
#include "proj/webmercator.h"
#include "proj/transversemercator.h"
#include "proj/lambertconic.h"
#include "proj/albersequal.h"
#include "proj/lambertazimuthal.h"
#include "proj/krovak.h"
#include "proj/polarstereographic.h"
#include "proj/obliquestereographic.h"
#include "proj/polyconic.h"
#include "proj/latlon.h"
#include "datum.h"
#include "gcs.h"
#include "pcs.h"
#include "projection.h"


Projection::Method::Method(int id)
{
	switch (id) {
		case 1024:
		case 1041:
		case 9801:
		case 9802:
		case 9804:
		case 9807:
		case 9809:
		case 9815:
		case 9818:
		case 9819:
		case 9820:
		case 9822:
		case 9829:
			_id = id;
			break;
		default:
			_id = 0;
	}
}

Projection::Projection(const PCS &pcs)
	: _gcs(pcs.gcs()), _ct(0), _units(pcs.conversion().units()),
	_cs(pcs.conversion().cs()), _geographic(false)
{
	const Ellipsoid &ellipsoid = _gcs.datum().ellipsoid();
	const Projection::Setup &setup = pcs.conversion().setup();

	switch (pcs.conversion().method().id()) {
		case 1024:
			_ct = new WebMercator();
			break;
		case 1041:
			_ct = new KrovakNE(ellipsoid, setup.standardParallel1(),
			  setup.standardParallel2(), setup.scale(), setup.latitudeOrigin(),
			  setup.longitudeOrigin(), setup.falseEasting(),
			  setup.falseNorthing());
			break;
		case 9801:
		case 9815: // Oblique mercator aproximation using LCC1
			_ct = new LambertConic1(ellipsoid, setup.latitudeOrigin(),
			  setup.longitudeOrigin(), setup.scale(), setup.falseEasting(),
			  setup.falseNorthing());
			break;
		case 9802:
			_ct = new LambertConic2(ellipsoid, setup.standardParallel1(),
			  setup.standardParallel2(), setup.latitudeOrigin(),
			  setup.longitudeOrigin(), setup.falseEasting(),
			  setup.falseNorthing());
			break;
		case 9804:
			_ct = new Mercator(ellipsoid, setup.latitudeOrigin(),
			  setup.longitudeOrigin(), setup.falseEasting(),
			  setup.falseNorthing());
			break;
		case 9807:
			_ct = new TransverseMercator(ellipsoid, setup.latitudeOrigin(),
			  setup.longitudeOrigin(), setup.scale(), setup.falseEasting(),
			  setup.falseNorthing());
			break;
		case 9809:
			_ct = new ObliqueStereographic(ellipsoid, setup.latitudeOrigin(),
			  setup.longitudeOrigin(), setup.scale(), setup.falseEasting(),
			  setup.falseNorthing());
			break;
		case 9818:
			_ct = new Polyconic(ellipsoid, setup.latitudeOrigin(),
			  setup.longitudeOrigin(), setup.falseEasting(),
			  setup.falseNorthing());
			break;
		case 9819:
			_ct = new Krovak(ellipsoid, setup.standardParallel1(),
			  setup.standardParallel2(), setup.scale(), setup.latitudeOrigin(),
			  setup.longitudeOrigin(), setup.falseEasting(),
			  setup.falseNorthing());
			break;
		case 9820:
			_ct = new LambertAzimuthal(ellipsoid, setup.latitudeOrigin(),
			  setup.longitudeOrigin(), setup.falseEasting(),
			  setup.falseNorthing());
			break;
		case 9822:
			_ct = new AlbersEqual(ellipsoid, setup.standardParallel1(),
			  setup.standardParallel2(), setup.latitudeOrigin(),
			  setup.longitudeOrigin(), setup.falseEasting(),
			  setup.falseNorthing());
			break;
		case 9829:
			_ct = new PolarStereographic(ellipsoid, setup.latitudeOrigin(),
			  setup.longitudeOrigin(), setup.falseEasting(),
			  setup.falseNorthing());
			break;
		default:
			_ct = 0;
	}
}

Projection::Projection(const GCS &gcs, const CoordinateSystem &cs)
  : _gcs(gcs), _units(LinearUnits(9001)), _cs(cs), _geographic(true)
{
	_ct = new LatLon(gcs.angularUnits());
}

Projection::Projection(const Projection &p)
{
	_gcs = p._gcs;
	_units = p._units;
	_ct = p._ct ? p._ct->clone() : 0;
	_geographic = p._geographic;
	_cs = p._cs;
}

Projection::~Projection()
{
	delete _ct;
}

Projection &Projection::operator=(const Projection &p)
{
	if (this != &p) {
		delete _ct;

		_gcs = p._gcs;
		_units = p._units;
		_ct = p._ct ? p._ct->clone() : 0;
		_geographic = p._geographic;
		_cs = p._cs;
	}

	return *this;
}

bool Projection::operator==(const Projection &p) const
{
	if (!isValid() || !p.isValid())
		return false;

	return (*_ct == *p._ct && _gcs == p._gcs && _units == p._units
	  && _cs == p._cs && _geographic == p._geographic);
}

PointD Projection::ll2xy(const Coordinates &c) const
{
	Q_ASSERT(isValid());
	return _units.fromMeters(_ct->ll2xy(_gcs.fromWGS84(c)));
}

Coordinates Projection::xy2ll(const PointD &p) const
{
	Q_ASSERT(isValid());
	return _gcs.toWGS84(_ct->xy2ll(_units.toMeters(p)));
}

#ifndef QT_NO_DEBUG
QDebug operator<<(QDebug dbg, const Projection::Setup &setup)
{
	dbg.nospace() << "Setup(" << setup.latitudeOrigin() << ", "
	  << setup.longitudeOrigin() << ", " << setup.scale() << ", "
	  << setup.falseEasting() << ", " << setup.falseNorthing() << ", "
	  << setup.standardParallel1() << ", " << setup.standardParallel2() << ")";
	return dbg.space();
}

QDebug operator<<(QDebug dbg, const Projection::Method &method)
{
	dbg.nospace() << "Method(" << method.id() << ")";
	return dbg.space();
}
#endif // QT_NO_DEBUG
