#ifndef SAMPLING_H
#define SAMPLING_H

#include "core/common/common_types.h"

#include <QVector3D>
#include <QVector2D>

#include <QString>
#include <QDebug>

namespace Sampling {

inline SamplingSpline Line_Sampling(const Vector3 start, const Vector3 end, const int split){

    SamplingSpline return_sampling_line;

    float x_step = ( end.x() - start.x() )/split;
    float y_step = ( end.y() - start.y() )/split;
    float z_step = ( end.z() - start.z() )/split;

    for(int i=0; i <= split; i++){

        SamplingMeshPoint* meshpoint = new SamplingMeshPoint({ start.x() + i * x_step, start.y() + i * y_step, start.z() + i * z_step });

        return_sampling_line.meshpoint_list.push_back( meshpoint );
        if (i!=0){

            if ( !return_sampling_line.meshpoint_list[i-1]->neighbor_meshpoints.contains(return_sampling_line.meshpoint_list[i]) ){

                return_sampling_line.meshpoint_list[i-1]->neighbor_meshpoints.insert( return_sampling_line.meshpoint_list[i] );

            }

            if ( !return_sampling_line.meshpoint_list[i]->neighbor_meshpoints.contains(return_sampling_line.meshpoint_list[i-1]) ){

                return_sampling_line.meshpoint_list[i]->neighbor_meshpoints.insert( return_sampling_line.meshpoint_list[i-1] );

            }
        }

};

    return return_sampling_line;
}

inline void Plane_Connector (SamplingPlane plane){

    if ( plane.next == nullptr ){ return; }

    else {

        SamplingPlane* next_plane = plane.next;

        SamplingSpline* ptr = plane.spline_starter;
        while ( ptr->next != nullptr ) {

            for(int i = 0; i< int(ptr->meshpoint_list.size()); i++){

                ptr->meshpoint_list[i]->neighbor_meshpoints.insert( next_plane->spline_starter->meshpoint_list[i] );

                next_plane->spline_starter->meshpoint_list[i]->neighbor_meshpoints.insert( ptr->meshpoint_list[i] );
            }

            ptr = ptr->next;
        }

        return Plane_Connector( *plane.next );

    }
}

}
#endif // SAMPLING_H
