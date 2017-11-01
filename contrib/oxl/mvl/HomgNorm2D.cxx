// This is oxl/mvl/HomgNorm2D.cxx
#ifdef VCL_NEEDS_PRAGMA_INTERFACE
#pragma implementation
#endif
//:
// \file

#include "HomgNorm2D.h"
#include <vcl_iostream.h>
#include <vnl/vnl_math.h>
#include <vgl/vgl_homg_point_2d.h>
#include <vgl/algo/vgl_homg_operators_2d.h> // for matrix * vgl_homg_point_2d

// ctor
HomgNorm2D::HomgNorm2D(vcl_vector<vgl_homg_point_2d<double> > const& points, bool unit_omega):
  normalized_(points.size()),
  unit_omega_(unit_omega)
{
  normalize(points);
}

HomgNorm2D::HomgNorm2D(const vcl_vector<HomgPoint2D>& points, bool unit_omega):
  normalized_(points.size()),
  unit_omega_(unit_omega)
{
  normalize(points);
}

// Destructor
HomgNorm2D::~HomgNorm2D()
{
}

static bool paranoid = true;
static void centre(const vcl_vector<HomgPoint2D>& in, vcl_vector<HomgPoint2D>& out, double* cx_out, double* cy_out);
static void centre(vcl_vector<vgl_homg_point_2d<double> > const& in, vcl_vector<HomgPoint2D>& out, double&, double&);
static double scale_xyroot2(const vcl_vector<HomgPoint2D>& in, vcl_vector<HomgPoint2D>& out);

//: Put the cog at the origin.
// Scale x,y so that mean (x^2 + y^2) = 2 when z = 1.

void HomgNorm2D::normalize(vcl_vector<vgl_homg_point_2d<double> > const& points)
{
  // from ho_trivechomg_normalise
  normalized_.resize(points.size());

  double cx, cy;
  centre(points, normalized_, cx, cy);

  double diameter = scale_xyroot2(normalized_, normalized_);
  if (diameter == 0) {
    vcl_cerr << "HomgNorm2D: All points coincident\n";
    diameter = 1;
    was_coincident_=true;
  }
  else
    was_coincident_=false;

  SimilarityMetric::set_center_and_scale(cx, cy, 1/diameter);

  if (paranoid) {
    SimilarityMetric::scale_matrices(1/diameter);
    for (unsigned i = 0; i < points.size(); ++i) {
      vnl_double_3 ni = normalized_[i].get_vector();
      vgl_homg_point_2d<double> mi = SimilarityMetric::imagehomg_to_homg(points[i]);
      ni[0] -= mi.x(); ni[1] -= mi.y(); ni[2] -= mi.w(); 
      double l = ni.magnitude();
      if (l > 1e-12) {
        vcl_cerr << "HomgNorm2D: "
                 << "d = " << diameter
                 << ", ni = " << normalized_[i].get_vector()
                 << ", mi = " << mi
                 << ", Residual = " << ni
                 << " (mag = " << l << ")\n";
      }
    }
  }

  if (unit_omega_) {
    /* homogeneous scale factor is normally undetermined - set it here though. */
    for (unsigned i = 0; i < normalized_.size(); ++i) {
      HomgPoint2D& p = normalized_[i];
      double x = p.x();
      double y = p.y();
      double w = p.w();

      if (w == 0) {
        /* average magnitude of finite points is root(3) after this stage - do the same for pts at infinity. */

        double scaling = vcl_sqrt(3.0) / vnl_math_hypot(x, y);
        x *= scaling;
        y *= scaling;
      } else {
        x /= w;
        y /= w;
        w = 1.0;
      }
      p.set(x,y,w);
    }
  }
}

void HomgNorm2D::normalize(const vcl_vector<HomgPoint2D>& points)
{
  // from ho_trivechomg_normalise
  normalized_.resize(points.size());

#if 0
  for (unsigned i = 0; i < points.size(); ++i)
    vcl_cerr << points[i].get_vector() << vcl_endl;
#endif

  double cx, cy;
  centre(points, normalized_, &cx, &cy);

  double diameter = scale_xyroot2(normalized_, normalized_);
  if (diameter == 0) {
    vcl_cerr << "HomgNorm2D: All points coincident\n";
    diameter = 1;
    was_coincident_=true; // FSM
  }
  else
    was_coincident_=false;

  SimilarityMetric::set_center_and_scale(cx, cy, 1/diameter);

  //vcl_cerr << "NORM = " << norm_matrix_ << vcl_endl;
  if (paranoid) {
    SimilarityMetric::scale_matrices(1/diameter);
    for (unsigned i = 0; i < points.size(); ++i) {
      vnl_double_3 ni = normalized_[i].get_vector();
      vnl_double_3 mi = SimilarityMetric::imagehomg_to_homg(points[i]).get_vector();
      vnl_double_3 residual = ni - mi;
      double l = residual.magnitude();
      if (l > 1e-12) {
     // vcl_cerr << "\n\n";
        vcl_cerr << "HomgNorm2D: "
                 << "d = " << diameter
                 << "ni = " << ni
                 << "mi = " << mi
                 << "Residual = " << residual
                 << " mag = " << l << vcl_endl;
      }
    }
  }

  if (unit_omega_) {
    /* homogeneous scale factor is normally undetermined - set it here though. */
    for (unsigned i = 0; i < normalized_.size(); ++i) {
      HomgPoint2D& p = normalized_[i];
      double x = p.x();
      double y = p.y();
      double w = p.w();

      if (w == 0) {
        /* average magnitude of finite points is root(3) after this stage - do the same for pts at infinity. */

        double scaling = vcl_sqrt(3.0) / vnl_math_hypot(x, y);
        x *= scaling;
        y *= scaling;
      } else {
        x /= w;
        y /= w;
        w = 1.0;
      }
      p.set(x,y,w);
    }
  }
}

static void centre(const vcl_vector<HomgPoint2D>& in,
                   vcl_vector<HomgPoint2D>& out,
                   double *cx, double *cy)
{
  // Compute center of mass
  double cog_x = 0;
  double cog_y = 0;
  unsigned cog_count = 0;
  unsigned n = in.size();
  for (unsigned i = 0; i < n; ++i) {
    const HomgPoint2D& p = in[i];
    double x,y;
    if (p.get_nonhomogeneous(x, y)) {
      cog_x += x;
      cog_y += y;
      ++cog_count;
    }
  }
  if (cog_count > 0) {
    cog_x /= (double) cog_count;
    cog_y /= (double) cog_count;
  }

  // Remember cog for caller
  *cx = cog_x;
  *cy = cog_y;

  // Build conditioning matrix
  vnl_double_3x3 C;
  C.set_identity();
  C(0,2) = -cog_x;
  C(1,2) = -cog_y;

  // Transform points
  {
    for (unsigned i = 0; i < n; ++i)
      out[i].set(C * in[i].get_vector());
  }

  // Check transformation has worked.
  if (paranoid) {
    cog_x = 0;
    cog_y = 0;
    for (unsigned i = 0; i < n; ++i) {
      const HomgPoint2D& p = out[i];
      double x,y;
      if (p.get_nonhomogeneous(x, y)) {
        cog_x += x;
        cog_y += y;
      }
    }
    if (vnl_math_hypot(cog_x, cog_y) > 1e-10)
      vcl_cerr << "HomgNorm2D: expected (0,0) computed (" << cog_x << "," << cog_y << ")\n";
  }
}

static void centre(vcl_vector<vgl_homg_point_2d<double> > const& in,
                   vcl_vector<HomgPoint2D>& out,
                   double& cx, double& cy)
{
  // Compute center of mass
  double cog_x = 0;
  double cog_y = 0;
  unsigned cog_count = 0;
  unsigned n = in.size();
  for (unsigned i = 0; i < n; ++i) {
    vgl_homg_point_2d<double> const& p = in[i];
    if (p.w() != 0.0) {
      cog_x += p.x()/p.w();
      cog_y += p.y()/p.w();
      ++cog_count;
    }
  }
  if (cog_count > 0) {
    cog_x /= (double) cog_count;
    cog_y /= (double) cog_count;
  }

  // Remember cog for caller
  cx = cog_x;
  cy = cog_y;

  // Build conditioning matrix
  vnl_double_3x3 C;
  C.set_identity();
  C(0,2) = -cog_x;
  C(1,2) = -cog_y;

  // Transform points
  for (unsigned i = 0; i < n; ++i)
  {
    vgl_homg_point_2d<double> q = C * in[i];
    out[i].set(q.x(),q.y(),q.w());
  }

  // Check transformation has worked.
  if (paranoid) {
    cog_x = 0;
    cog_y = 0;
    for (unsigned i = 0; i < n; ++i) {
      const HomgPoint2D& p = out[i];
      double x,y;
      if (p.get_nonhomogeneous(x, y)) {
        cog_x += x;
        cog_y += y;
      }
    }
    if (vnl_math_hypot(cog_x, cog_y) > 1e-10)
      vcl_cerr << "HomgNorm2D: expected (0,0) computed (" << cog_x << "," << cog_y << ")\n";
  }
}

// - Scale the x and y components so that the average bivec magnitude is vcl_sqrt(2).
// Scale x,y so that mean (x^2 + y^2) = 2 when z = 1.

static double scale_xyroot2(const vcl_vector<HomgPoint2D>& in,
                            vcl_vector<HomgPoint2D>& out)
{
  double magnitude = 0;
  unsigned numfinite = 0;
  for (unsigned i = 0; i < in.size(); ++i) {
    const HomgPoint2D& p = in[i];
    double x,y;
    if (p.get_nonhomogeneous(x, y)) {
      magnitude += vnl_math_hypot(x, y);
      ++numfinite;
    }
  }

  // Compute diameter
  double diameter = 1;
  if (numfinite > 0) {
    magnitude /= (double) numfinite;
    diameter = magnitude / vcl_sqrt(2.0);
  }

  // Scale the points
  {
    for (unsigned i = 0; i < in.size(); ++i)
      out[i].set(in[i].x(), in[i].y(), in[i].w() * diameter);
  }

  if (paranoid) {
    magnitude = 0;
    numfinite = 0;
    for (unsigned i = 0; i < in.size(); ++i) {
      const HomgPoint2D& p = out[i];
      double x,y;
      if (p.get_nonhomogeneous(x, y)) {
        magnitude += vnl_math_hypot(x, y);
        ++numfinite;
      }
    }
    if (numfinite > 0) magnitude /= numfinite;
    const double expected = vcl_sqrt(2.0);
    if (vnl_math_abs(expected - magnitude) > 1e-13) // 1e-14 gave false alarm -- fsm
      vcl_cerr << "HomgNorm2D: Expected magnitude " << expected << " computed magnitude " << magnitude << vcl_endl;
  }

  // Return
  return diameter;
}
