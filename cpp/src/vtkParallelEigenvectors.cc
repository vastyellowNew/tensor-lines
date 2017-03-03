#include "vtkParallelEigenvectors.hh"

#include "ParallelEigenvectors.hh"

#include <Eigen/Geometry>

#include <vtkPolyData.h>
#include <vtkUnstructuredGrid.h>
#include <vtkDataSetAttributes.h>
#include <vtkSmartPointer.h>
#include <vtkCellIterator.h>
#include <vtkTetra.h>
#include <vtkGenericCell.h>
#include <vtkCellArray.h>
#include <vtkMergePoints.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkIdList.h>

#include <vtkCommand.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkObjectFactory.h>
#include <vtkStreamingDemandDrivenPipeline.h>

#include <map>
#include <vector>
#include <iostream>

vtkStandardNewMacro(vtkParallelEigenvectors);

vtkParallelEigenvectors::vtkParallelEigenvectors()
{
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(2);
  this->SetInputArrayToProcess(0, 0, 0,
                               vtkDataObject::FIELD_ASSOCIATION_POINTS,
                               vtkDataSetAttributes::TENSORS);
  this->SetInputArrayToProcess(1, 0, 0,
                               vtkDataObject::FIELD_ASSOCIATION_POINTS,
                               vtkDataSetAttributes::TENSORS);
}


vtkParallelEigenvectors::~vtkParallelEigenvectors()
{
}


// void vtkParallelEigenvectors::PrintSelf(ostream& os, vtkIndent indent)
// {
//     this->Superclass::PrintSelf(os, indent);
// }


vtkPolyData* vtkParallelEigenvectors::GetOutput()
{
    return this->GetOutput(0);
}


vtkPolyData* vtkParallelEigenvectors::GetOutput(int port)
{
    return vtkPolyData::SafeDownCast(this->GetOutputDataObject(port));
}

int vtkParallelEigenvectors::ProcessRequest(
        vtkInformation* request,
        vtkInformationVector** inputVector,
        vtkInformationVector* outputVector)
{
  // Create an output object of the correct type.
    if(request->Has(vtkDemandDrivenPipeline::REQUEST_DATA_OBJECT()))
    {
        return this->RequestDataObject(request, inputVector, outputVector);
    }
    // generate the data
    if(request->Has(vtkDemandDrivenPipeline::REQUEST_DATA()))
    {
        return this->RequestData(request, inputVector, outputVector);
    }

    if(request->Has(vtkStreamingDemandDrivenPipeline::REQUEST_UPDATE_EXTENT()))
    {
        return this->RequestUpdateExtent(request, inputVector, outputVector);
    }

    // execute information
    if(request->Has(vtkDemandDrivenPipeline::REQUEST_INFORMATION()))
    {
        return this->RequestInformation(request, inputVector, outputVector);
    }

    return this->Superclass::ProcessRequest(request, inputVector, outputVector);
}

int vtkParallelEigenvectors::FillOutputPortInformation(
    int port, vtkInformation* info)
{
    // now add our info
    if(port == 0)
    {
        info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkPolyData");
    }
    else if(port == 1)
    {
        info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkUnstructuredGrid");
    }
    return 1;
}


int vtkParallelEigenvectors::FillInputPortInformation(
        int vtkNotUsed(port), vtkInformation* info)
{
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkUnstructuredGrid");
    return 1;
}


int vtkParallelEigenvectors::RequestDataObject(
        vtkInformation* vtkNotUsed(request),
        vtkInformationVector** vtkNotUsed(inputVector),
        vtkInformationVector* outputVector )
{
    // RequestDataObject (RDO) is an earlier pipeline pass. During RDO, each
    // filter is supposed to produce an empty data object of the proper type

    auto* outInfo = outputVector->GetInformationObject(0);
    auto* output = vtkPolyData::SafeDownCast(
            outInfo->Get(vtkDataObject::DATA_OBJECT()));

    auto* outInfo2 = outputVector->GetInformationObject(1);
    auto* output2 = vtkUnstructuredGrid::SafeDownCast(
            outInfo2->Get(vtkDataObject::DATA_OBJECT()));

    if(!output)
    {
        output = vtkPolyData::New();
        outInfo->Set(vtkDataObject::DATA_OBJECT(), output);
        output->FastDelete();

        this->GetOutputPortInformation(0)->Set(
                vtkDataObject::DATA_EXTENT_TYPE(), output->GetExtentType());
    }
    if(!output2)
    {
        output2 = vtkUnstructuredGrid::New();
        outInfo2->Set(vtkDataObject::DATA_OBJECT(), output2);
        output2->FastDelete();

        this->GetOutputPortInformation(1)->Set(
                vtkDataObject::DATA_EXTENT_TYPE(), output2->GetExtentType());
    }

    return 1;
}


int vtkParallelEigenvectors::RequestInformation(
        vtkInformation* vtkNotUsed(request),
        vtkInformationVector** vtkNotUsed(inputVector),
        vtkInformationVector* vtkNotUsed(outputVector))
{
    return 1;
}


int vtkParallelEigenvectors::RequestUpdateExtent(
    vtkInformation* vtkNotUsed(request),
    vtkInformationVector** inputVector,
    vtkInformationVector* vtkNotUsed(outputVector))
{
    auto numInputPorts = this->GetNumberOfInputPorts();
    for(auto i = 0; i < numInputPorts; i++)
    {
        auto numInputConnections = this->GetNumberOfInputConnections(i);
        for(auto j = 0; j < numInputConnections; j++)
        {
            auto* inputInfo = inputVector[i]->GetInformationObject(j);
            inputInfo->Set(vtkStreamingDemandDrivenPipeline::EXACT_EXTENT(), 1);
        }
    }
    return 1;
}


int vtkParallelEigenvectors::RequestData(
    vtkInformation* vtkNotUsed(request),
    vtkInformationVector **inputVector,
    vtkInformationVector* outputVector )
{
    using namespace peigv;

    auto* outInfo = outputVector->GetInformationObject(0);
    auto* output = vtkPolyData::SafeDownCast(
            outInfo->Get(vtkDataObject::DATA_OBJECT()));

    auto* outInfo2 = outputVector->GetInformationObject(1);
    auto* output2 = vtkUnstructuredGrid::SafeDownCast(
            outInfo2->Get(vtkDataObject::DATA_OBJECT()));

    auto* inInfo = inputVector[0]->GetInformationObject(0);
    auto* input = vtkUnstructuredGrid::SafeDownCast(
            inInfo->Get(vtkDataObject::DATA_OBJECT()));

    // Get the two DataArrays corresponding to the tensor data
    auto* array1 = this->GetInputArrayToProcess(0, inputVector);
    auto* array2 = this->GetInputArrayToProcess(1, inputVector);

    // Check if the data arrays have the correct number of components
    if(array1->GetNumberOfComponents() != 9
       || array2->GetNumberOfComponents() != 9)
    {
        vtkErrorMacro(<<"both input arrays must be tensors with 9 components.");
        return 0;
    }

    if(this->GetInputArrayInformation(0)->Get(vtkDataObject::FIELD_ASSOCIATION())
       != vtkDataObject::FIELD_ASSOCIATION_POINTS
       || this->GetInputArrayInformation(1)->Get(vtkDataObject::FIELD_ASSOCIATION())
            != vtkDataObject::FIELD_ASSOCIATION_POINTS)
    {
        vtkErrorMacro(<<"both input arrays must be point data.");
        return 0;
    }

    // Point and CellArrays for output dataset
    output->SetPoints(vtkPoints::New());
    output->SetVerts(vtkCellArray::New());

    // Filter for automatically merging identical points
    // auto locator = vtkSmartPointer<vtkMergePoints>::New();
    // locator->InitPointInsertion(output->GetPoints(), input->GetBounds());

    // Output arrays for point information
    auto eig_rank1 = vtkSmartPointer<vtkDoubleArray>::New();
    eig_rank1->SetName("Rank1");
    output->GetPointData()->AddArray(eig_rank1);
    auto eig_rank2 = vtkSmartPointer<vtkDoubleArray>::New();
    eig_rank2->SetName("Rank2");
    output->GetPointData()->AddArray(eig_rank2);
    auto eival1 = vtkSmartPointer<vtkDoubleArray>::New();
    eival1->SetName("Eigenvalue 1");
    output->GetPointData()->AddArray(eival1);
    auto eival2 = vtkSmartPointer<vtkDoubleArray>::New();
    eival2->SetName("Eigenvalue 2");
    output->GetPointData()->AddArray(eival2);
    auto eivec = vtkSmartPointer<vtkDoubleArray>::New();
    eivec->SetName("Eigenvector");
    eivec->SetNumberOfComponents(3);
    output->GetPointData()->SetVectors(eivec);

    auto n_neighbors = vtkSmartPointer<vtkDoubleArray>::New();
    n_neighbors->SetName("Neighbor Cells");
    output->GetPointData()->AddArray(n_neighbors);

    // Output 2 for debug purposes
    auto opoints = vtkSmartPointer<vtkPoints>::New();
    opoints->DeepCopy(input->GetPoints());
    output2->SetPoints(opoints);
    auto osfield = vtkSmartPointer<vtkDoubleArray>::New();
    osfield->SetName(array1->GetName());
    osfield->DeepCopy(array1);
    output2->GetPointData()->AddArray(osfield);
    auto otfield = vtkSmartPointer<vtkDoubleArray>::New();
    otfield->SetName(array2->GetName());
    otfield->DeepCopy(array2);
    output2->GetPointData()->AddArray(otfield);
    auto origID = vtkSmartPointer<vtkDoubleArray>::New();
    origID->SetName("Original Cell ID");
    output2->GetCellData()->AddArray(origID);
    auto npoints_field = vtkSmartPointer<vtkDoubleArray>::New();
    npoints_field->SetName("NPoints");
    output2->GetCellData()->SetScalars(npoints_field);

    output2->Allocate();


    // Structure for mapping cell IDs to parallel eigenvector points
    auto cell_map = std::map<vtkIdType, vtkSmartPointer<vtkIdList>>{};

    auto ref_vec = vec3d::Random().eval();

    auto it = vtkSmartPointer<vtkCellIterator>(input->NewCellIterator());
    auto current_cell = 0;
    for(it->InitTraversal(); !it->IsDoneWithTraversal(); it->GoToNextCell())
    {
        if(it->GetCellType() != VTK_TETRA)
        {
            continue;
        }

        auto* cell_points = it->GetPoints();
        auto* point_ids = it->GetPointIds();
        assert(it->GetNumberOfPoints() == 4);

        std::cout << "Processing Cell " << it->GetCellId() << "\n";

        using mat3dm = Eigen::Map<peigv::mat3d>;
        using vec3dm = Eigen::Map<peigv::vec3d>;

        // Add parallel eigenvector points on the face with the given indices
        // to the output dataset
        auto add_peigv_points = [&](int i1, int i2, int i3)
        {
            auto p1 = vec3d{vec3dm{cell_points->GetPoint(i1)}};
            auto p2 = vec3d{vec3dm{cell_points->GetPoint(i2)}};
            auto p3 = vec3d{vec3dm{cell_points->GetPoint(i3)}};

            // Compute face normal
            auto normal = vec3d{(p1 - p2).cross(p3 - p2).normalized()};

            // Only process each face once
            // if(normal.dot(ref_vec) < 0) return;

            auto s1 = mat3d{mat3dm{array1->GetTuple(point_ids->GetId(i1))}};
            auto s2 = mat3d{mat3dm{array1->GetTuple(point_ids->GetId(i2))}};
            auto s3 = mat3d{mat3dm{array1->GetTuple(point_ids->GetId(i3))}};

            auto t1 = mat3d{mat3dm{array2->GetTuple(point_ids->GetId(i1))}};
            auto t2 = mat3d{mat3dm{array2->GetTuple(point_ids->GetId(i2))}};
            auto t3 = mat3d{mat3dm{array2->GetTuple(point_ids->GetId(i3))}};

            auto face = vtkSmartPointer<vtkIdList>::New();
            face->SetNumberOfIds(3);
            face->SetId(0, point_ids->GetId(i1));
            face->SetId(1, point_ids->GetId(i2));
            face->SetId(2, point_ids->GetId(i3));

            auto cells = vtkSmartPointer<vtkIdList>::New();
            input->GetCellNeighbors(it->GetCellId(), face, cells);

            auto points = peigv::findParallelEigenvectors(
                    s1, s2, s3, t1, t2, t3, p1, p2, p3,
                    this->GetSpatialEpsilon(), this->GetDirectionEpsilon(),
                    this->GetClusterEpsilon(), this->GetParallelityEpsilon());
            if(!points.empty())
            {
                // std::cout << "\nCell has " << cells->GetNumberOfIds() << " neighboring cells for face" << std::endl;
                // std::cout << "\nFound " << points.size() << " points" << std::endl;
            }
            for(const auto& p: points)
            {
                auto pid = output->GetPoints()->InsertNextPoint(p.pos.data());
                eig_rank1->InsertValue(pid, double(p.s_rank));
                eig_rank2->InsertValue(pid, double(p.t_rank));
                eival1->InsertValue(pid, p.s_eival);
                eival2->InsertValue(pid, p.t_eival);
                eivec->InsertTuple(pid, p.eivec.data());
                n_neighbors->InsertValue(pid, double(cells->GetNumberOfIds()));
                output->InsertNextCell(VTK_VERTEX, 1, &pid);
                if(!cell_map[it->GetCellId()].Get())
                {
                    cell_map[it->GetCellId()] = vtkSmartPointer<vtkIdList>::New();
                }
                cell_map[it->GetCellId()]->InsertNextId(pid);
                // for(auto i: range(cells->GetNumberOfIds()))
                // {
                //     if(!cell_map[cells->GetId(i)].Get())
                //     {
                //         cell_map[cells->GetId(i)] = vtkSmartPointer<vtkIdList>::New();
                //     }
                //     cell_map[cells->GetId(i)]->InsertNextId(pid);
                //     std::cout << "Neighbor Cell: " << cells->GetId(i) << std::endl;
                // }
            }
        };
        std::cout << "\nFace 1: " << std::endl;
        add_peigv_points(0, 1, 2);
        std::cout << "\nFace 2: " << std::endl;
        add_peigv_points(1, 3, 2);
        std::cout << "\nFace 3: " << std::endl;
        add_peigv_points(0, 3, 1);
        std::cout << "\nFace 4: " << std::endl;
        add_peigv_points(0, 2, 3);

        ++current_cell;
        this->UpdateProgress(double(current_cell)
                             / double(input->GetNumberOfCells()));
    }

    // For cells with exactly two parallel eigenvector points, connect them
    // with a line
    output->SetLines(vtkCellArray::New());

    for(const auto& c: cell_map)
    {
        auto points = vtkSmartPointer<vtkIdList>::New();
        input->GetCellPoints(c.first, points);
        auto cid = output2->InsertNextCell(input->GetCellType(c.first),
                                           points);
        npoints_field->InsertValue(cid, double(c.second->GetNumberOfIds()));
        origID->InsertValue(cid, double(c.first));

        if(c.second->GetNumberOfIds() == 2)
        {
            output->InsertNextCell(VTK_LINE, c.second);
        }
        else
        {
            std::cout << "Cell has " << c.second->GetNumberOfIds() << " points" << std::endl;
        }
    }

    return 1;
}
