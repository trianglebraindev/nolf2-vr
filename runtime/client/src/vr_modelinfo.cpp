//------------------------------------------------------------------
//
//  MODULE  : VR_MODELINFO.CPP
//
//  PURPOSE : NOLF2 VR. Writes the make-up of a model (its pieces,
//            skeleton and sockets) to vr_models.log in the run folder,
//            so we can see how the first-person gun models are built:
//            which pieces are the arms, which bones are the hands,
//            where the sights and muzzle are.
//
//  Exported from Lithtech.exe (see sdk/inc/ltvr.h, LTVR_LogModel).
//
//------------------------------------------------------------------

#include "bdefs.h"
#include "de_objects.h"
#include "model.h"

#include <stdio.h>

extern "C" __declspec(dllexport) void LTVR_LogModel(HOBJECT hObj, const char* pszTag)
{
	if (!hObj || hObj->m_ObjectType != OT_MODEL)
		return;
	ModelInstance* pInst = (ModelInstance*)hObj;
	Model* pModel = pInst->GetModelDB();
	if (!pModel)
		return;

	FILE* pFile = fopen("vr_models.log", "a");
	if (!pFile)
		return;

	fprintf(pFile, "==== %s\n", pszTag ? pszTag : "?");

	fprintf(pFile, "Pieces (%u):\n", (unsigned)pModel->NumPieces());
	for (uint32 i = 0; i < pModel->NumPieces(); ++i)
	{
		ModelPiece* pPiece = pModel->GetPiece(i);
		fprintf(pFile, "  %2u  %s\n", (unsigned)i, (pPiece && pPiece->GetName()) ? pPiece->GetName() : "?");
	}

	// Bind pose: where each bone sits when the model isn't animated, in
	// model space.
	fprintf(pFile, "Nodes (%u): index  name  parent  bind-pose position\n", (unsigned)pModel->NumNodes());
	for (uint32 i = 0; i < pModel->NumNodes(); ++i)
	{
		ModelNode* pNode = pModel->GetNode(i);
		if (!pNode)
			continue;
		LTVector vPos;
		pNode->GetGlobalTransform().GetTranslation(vPos);
		fprintf(pFile, "  %2u  %-28s parent %3d  %8.3f %8.3f %8.3f\n", (unsigned)i,
			pNode->GetName() ? pNode->GetName() : "?", (int)pNode->GetParentNodeIndex(), vPos.x, vPos.y, vPos.z);
	}

	fprintf(pFile, "Sockets (%u): name  node  offset from node\n", (unsigned)pModel->NumSockets());
	for (uint32 i = 0; i < pModel->NumSockets(); ++i)
	{
		ModelSocket* pSocket = pModel->GetSocket(i);
		if (!pSocket)
			continue;
		fprintf(pFile, "  %-28s node %3u  %8.3f %8.3f %8.3f\n", pSocket->GetName() ? pSocket->GetName() : "?",
			(unsigned)pSocket->m_iNode, pSocket->m_Pos.x, pSocket->m_Pos.y, pSocket->m_Pos.z);
	}
	fprintf(pFile, "\n");
	fclose(pFile);
}
