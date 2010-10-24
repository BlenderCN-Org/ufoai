/**
 * @file Plane3.h
 * @brief A plane in 3D space can be represented by a point and a normal vector.
 *
 * It is sufficient to specify four numbers to fully describe the plane: the three
 * components of the normal vector (x,y,z) and the dot product of the normal and any point of this plane.
 * (basically this is the "height" at which the plane intersects the z-axis)
 *
 * There are several constructors available: one requires all four number be passed directly,
 * the second requires the normal vector and the distance <dist> to be passed, the third and fourth
 * requires a set of three points that define the plane.
 *
 * Note: the plane numbers are stored in double precision.
 * Note: the constructor requiring three points does NOT check if two or more points are equal.
 * Note: two planes are considered equal when the difference of their normals and distances are below an epsilon.
 */

/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined(INCLUDED_MATH_PLANE_H)
#define INCLUDED_MATH_PLANE_H

#include "FloatTools.h"
#include "Vector3.h"
#include "matrix.h"

namespace {
// Some constants for "equality" check.
const double EPSILON_NORMAL = 0.0001;
const double EPSILON_DIST = 0.02;
}

class Plane3 {
	private:
		BasicVector3<double> _normal; // normal vector
		double _dist; // distance

	public:
		// Constructor with no arguments
		Plane3() {
		}

		// Constructor which expects four numbers, the first three are the components of the normal vector.
		Plane3(double nx, double ny, double nz, double dist) :
			_normal(nx, ny, nz), _dist(dist) {
		}

		// Construct a plane from any BasicVector3 and the distance <dist>
		template<typename Element>
		Plane3(const BasicVector3<Element>& normal, double dist) :
			_normal(normal), _dist(dist) {
		}

		// Construct a plane from three points <p0>, <p1> and <p2>
		template<typename Element>
		Plane3(const BasicVector3<Element>& p0, const BasicVector3<Element>& p1,
				const BasicVector3<Element>& p2) :
			_normal((p1 - p0).crossProduct(p2 - p0).getNormalised()), _dist(p0.dot(_normal)) {
		}

		// Construct a plane from three points (same as above, just with an array as argument
		template<typename Element>
		Plane3(const BasicVector3<Element> points[3]) :
			_normal((points[1] - points[2]).crossProduct(points[0] - points[2]).getNormalised()),
					_dist(points[2].dot(_normal)) {
		}

		//	The negation operator for this plane - the normal vector components and the distance are negated
		Plane3 operator-() const {
			return Plane3(-_normal, -_dist);
		}

		/**
		 * greebo: Note that planes are considered equal if their normal vectors and
		 * distances don't differ more than an epsilon value.
		 */
		bool operator==(const Plane3& other) const {
			return vector3_equal_epsilon(_normal, other._normal, EPSILON_NORMAL)
					&& float_equal_epsilon(_dist, other._dist, EPSILON_DIST);
		}

		// Returns the normal vector of this plane
		BasicVector3<double>& normal() {
			return _normal;
		}

		const BasicVector3<double>& normal() const {
			return _normal;
		}

		// Returns the distance of the plane (where the plane intersects the z-axis)
		double& dist() {
			return _dist;
		}

		const double& dist() const {
			return _dist;
		}

		/* greebo: This normalises the plane by turning the normal vector into a unit vector (dividing it by its length)
		 * and scaling the distance down by the same amount */
		Plane3 getNormalised() const {
			double rmagnitudeInv = 1 / _normal.getLength();
			return Plane3(_normal * rmagnitudeInv, _dist * rmagnitudeInv);
		}

		// Normalises this Plane3 object in-place
		void normalise() {
			double rmagnitudeInv = 1 / _normal.getLength();

			_normal *= rmagnitudeInv;
			_dist *= rmagnitudeInv;
		}

		// Reverses this plane, by negating all components
		void reverse() {
			_normal = -_normal;
			_dist = -_dist;
		}

		Plane3 getTranslated(const Vector3& translation) const {
			double distTransformed = -((-_dist * _normal.x() + translation.x()) * _normal.x()
					+ (-_dist * _normal.y() + translation.y()) * _normal.y() + (-_dist
					* _normal.z() + translation.z()) * _normal.z());
			return Plane3(_normal, distTransformed);
		}

		// Checks if the floats of this plane are valid, returns true if this is the case
		bool isValid() const {
			return float_equal_epsilon(_normal.dot(_normal), 1.0, 0.01);
		}

		/* greebo: Use this to calculate the projection of a <pointToProject> onto this plane.
		 *
		 * @returns: the Vector3 pointing to the point on the plane with the shortest
		 * distance from the passed <pointToProject> */
		Vector3 getProjection(const Vector3& pointToProject) const {
			// Get the normal vector of this plane and normalise it
			Vector3 n = _normal.getNormalised();

			// Retrieve a point of the plane
			Vector3 planePoint = n * _dist;

			// Calculate the projection and return it
			return pointToProject + planePoint - n * pointToProject.dot(n);
		}

		/** greebo: Returns the distance to the given point.
		 */
		double distanceToPoint(const Vector3& point) const {
			return point.dot(_normal) - _dist;
		}

		/* greebo: This calculates the intersection point of three planes.
		 * Returns <0,0,0> if no intersection point could be found, otherwise returns the coordinates of the intersection point
		 * (this may also be 0,0,0) */
		static Vector3 intersect(const Plane3& plane1, const Plane3& plane2, const Plane3& plane3) {
			const Vector3& n1 = plane1.normal();
			const Vector3& n2 = plane2.normal();
			const Vector3& n3 = plane3.normal();

			Vector3 n1n2 = n1.crossProduct(n2);
			Vector3 n2n3 = n2.crossProduct(n3);
			Vector3 n3n1 = n3.crossProduct(n1);

			double denom = n1.dot(n2n3);

			// Check if the denominator is zero (which would mean that no intersection is to be found
			if (denom != 0) {
				return (n2n3 * plane1.dist() + n3n1 * plane2.dist() + n1n2 * plane3.dist()) / denom;
			} else {
				// No intersection could be found, return <0,0,0>
				return Vector3(0, 0, 0);
			}
		}

}; // class Plane3

inline Plane3 plane3_translated(const Plane3& plane, const Vector3& translation) {
	Plane3 transformed;
	transformed.normal().x() = plane.normal().x();
	transformed.normal().y() = plane.normal().y();
	transformed.normal().z() = plane.normal().z();
	transformed.dist() = -((-plane.dist() * transformed.normal().x() + translation.x()) * transformed.normal().x() + (-plane.dist()
			* transformed.normal().y() + translation.y()) * transformed.normal().y() + (-plane.dist() * transformed.normal().z()
			+ translation.z()) * transformed.normal().z());
	return transformed;
}

inline Plane3 plane3_transformed(const Plane3& plane, const Matrix4& transform) {
	Plane3 transformed;
	transformed.normal().x() = transform[0] * plane.normal().x() + transform[4] * plane.normal().y() + transform[8] * plane.normal().z();
	transformed.normal().y() = transform[1] * plane.normal().x() + transform[5] * plane.normal().y() + transform[9] * plane.normal().z();
	transformed.normal().z() = transform[2] * plane.normal().x() + transform[6] * plane.normal().y() + transform[10] * plane.normal().z();
	transformed.dist() = -((-plane.dist() * transformed.normal().x() + transform[12]) * transformed.normal().x() + (-plane.dist()
			* transformed.normal().y() + transform[13]) * transformed.normal().y() + (-plane.dist() * transformed.normal().z()
			+ transform[14]) * transformed.normal().z());
	return transformed;
}

inline Plane3 plane3_inverse_transformed(const Plane3& plane, const Matrix4& transform) {
	return Plane3(transform[0] * plane.normal().x() + transform[1] * plane.normal().y() + transform[2] * plane.normal().z()
			+ transform[3] * plane.dist(), transform[4] * plane.normal().x() + transform[5] * plane.normal().y()
			+ transform[6] * plane.normal().z() + transform[7] * plane.dist(), transform[8] * plane.normal().x()
			+ transform[9] * plane.normal().y() + transform[10] * plane.normal().z() + transform[11] * plane.dist(),
			transform[12] * plane.normal().x() + transform[13] * plane.normal().y() + transform[14] * plane.normal().z()
					+ transform[15] * plane.dist());
}

inline Plane3 plane3_flipped(const Plane3& plane) {
	return Plane3(-plane.normal(), -plane.dist());
}

inline bool plane3_equal(const Plane3& self, const Plane3& other) {
	return vector3_equal_epsilon(self.normal(), other.normal(), EPSILON_NORMAL)
			&& float_equal_epsilon(self.dist(), other.dist(), EPSILON_DIST);
}

inline bool plane3_opposing(const Plane3& self, const Plane3& other) {
	return plane3_equal(self, plane3_flipped(other));
}

#endif
