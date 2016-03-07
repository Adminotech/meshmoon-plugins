// !ref: http://meshmoon.data.s3.amazonaws.com/app/lib/class.js, Script
// !ref: http://meshmoon.data.s3.amazonaws.com/app/lib/json2.js, Script
// !ref: http://meshmoon.data.s3.amazonaws.com/app/lib/admino-utils-common-deploy.js, Script
// !ref: http://meshmoon.data.s3.amazonaws.com/app/lib/meshmoon-rocket-deploy.js, Script

engine.IncludeFile("http://meshmoon.data.s3.amazonaws.com/app/lib/class.js");
engine.IncludeFile("http://meshmoon.data.s3.amazonaws.com/app/lib/json2.js");
engine.IncludeFile("http://meshmoon.data.s3.amazonaws.com/app/lib/admino-utils-common-deploy.js");

engine.ImportExtension("qt.core");
engine.ImportExtension("qt.gui");

var _p = null;
var envEntName = (scene.EntityByName("Environment") != null ? "Environment" : "RocketEnvironment");

var MovingLight = Class.extend(
{
    init : function(ent, moveTime)
    {        
        this.entity = ent;
        this.direction = 1;
        this.elapsedTime = 0.0;
        this.moveTime = moveTime;

        var dynamicComponent = ent.Component("DynamicComponent", "LightData");
        if (dynamicComponent != null)
        {
            if (dynamicComponent.moveTime != null)
                this.moveTime = dynamicComponent.moveTime;
            if (dynamicComponent.startPosition != null)
                this.startPosition = dynamicComponent.startPosition;
            if (dynamicComponent.endPosition != null)
                this.endPosition = dynamicComponent.endPosition;
        }
    },

    update : function(frameTime)
    {
        if (this.startPosition === undefined || this.endPosition === undefined)
            return;
    
        var t = this.entity.placeable.transform;
        
        if (this.direction == 1)
            t.pos = this.startPosition.Lerp(this.endPosition, this.elapsedTime / this.moveTime);
        else if (this.direction == -1)
            t.pos = this.endPosition.Lerp(this.startPosition, this.elapsedTime / this.moveTime);
        
        this.entity.placeable.transform = t;
        
        this.elapsedTime += frameTime;
        if (this.elapsedTime > this.moveTime)
        {
            this.elapsedTime = 0.0;
            this.direction = this.direction * -1;
        }
    }
});

var ShaderDemoClient = Class.extend(
{
    init : function()
    {
        this.runningId = 0;
        this.createdLocalEnts = [];
        this.waterAffectedEnts = [];

        this.setCameraStartPosition("FreeLookCamera");
        this.setCameraStartPosition("AdminoFreeCamera");

        var pos = new float3(3, 14, 12);
        var scale = new float3(7, 7, 7);
        var move = new float3(0, 0, -20);

        this.waterAffectedEnts.push(
            this.createMesh("Sphere", "Sphere.mesh", "Diffuse.material", 
                "Diffuse", pos, scale)
        );
        this.waterAffectedEnts.push(
            this.createMesh("Sphere", "Sphere.mesh", "DiffuseNormalMap.material", 
                "Diffuse +\n Normal map", pos.Add(move), scale)
        );
        this.waterAffectedEnts.push(
            this.createMesh("Sphere", "Sphere.mesh", "DiffuseNormalMapSpecularMap.material", 
                "Diffuse +\n Normal map +\n Specular map", pos.Add(move.Mul(2)), scale)
        );
        this.waterAffectedEnts.push(
            this.createMesh("Sphere", "Sphere.mesh", "DiffuseNormalMapSpecularMapLightMap.material", 
                "Diffuse +\n Normal map +\n Specular map +\n Light map", pos.Add(move.Mul(3)), scale)
        );

        this.waterHeightQueryOffsets = [
            new float3(0, 0, 0),
            new float3(-0.5, 0, -0.5),
            new float3(0.5, 0, -0.5),
            new float3(0.5, 0, 0.5),
            new float3(-0.5, 0, 0.5)
        ];

        this.onUpdate(0);
        frame.Updated.connect(this, this.onUpdate);
    },

    shutDown : function()
    {
        for (var i = 0; i < this.createdLocalEnts.length; i++)
        {
            scene.RemoveEntity(this.createdLocalEnts[i]);
        }
        this.createdLocalEnts = [];
    },

    setCameraStartPosition : function(cameraName)
    {
        var camera = scene.EntityByName(cameraName);
        if (camera != null)
            var dynComp = me.Component("DynamicComponent", "DemoData");
            if (dynComp != null)
                if (dynComp.cameraTransform != null)
                    camera.placeable.transform = dynComp.cameraTransform;
    },

    uniqueName : function(name)
    {
        this.runningId++;
        return name + "_" + this.runningId;
    },

    createMesh : function(name, meshRef, materialRefs, hoveringText, position, scale)
    {
        var ent = scene.CreateLocalEntity([ "Name", "Placeable", "Mesh", "Rigidbody" ], AttributeChange.LocalOnly, false, true);
        ent.name = this.uniqueName(name);

        ent.mesh.meshRef = meshRef;
        if (typeof materialRefs === "string")
            ent.mesh.materialRefs = [ materialRefs ];
        else
            ent.mesh.materialRefs = materialRefs;

        var t = ent.placeable.transform;
        if (position !== undefined)
            t.pos = position;
        if (scale !== undefined)
            t.scale = scale;

        t.rot.z = 10 + Math.random() * 20;
        t.rot.x = 10 + Math.random() * 20;
        t.rot.y = 10 + Math.random() * 20;
        ent.placeable.transform = t;

        if (hoveringText !== undefined && hoveringText !== "")
        {
            var hoveringTextEnt = scene.CreateLocalEntity(["Placeable", "HoveringText"], AttributeChange.LocalOnly, false, true);

            var textTransform = hoveringTextEnt.placeable.transform;
            textTransform.scale = new float3(7, 7, 7);
            hoveringTextEnt.placeable.transform = textTransform;

            hoveringTextEnt.name = ent.name + "_text";
            var hovering = hoveringTextEnt.Component("HoveringText");
            hovering.material = "HoveringText.material";
            hovering.text = hoveringText;
            hovering.fontColor = new Color(1,1,1);
            hovering.fontSize = 40;
            hovering.position = new float3(0,1.6,0);
            hovering.texWidth = 512;
            hovering.texHeight = 256;
            hovering.width = 2;
            this.createdLocalEnts.push(hoveringTextEnt.id);
        }

        ent.rigidBody.useGravity = false;
        ent.rigidBody.mass = 10.0;

        var envEnt = scene.EntityByName(envEntName);
        if (envEnt != null && envEnt.meshmoonSky != null)
        {
            var quat = new Quat(new float3(0, 1, 0), DegToRad(envEnt.meshmoonSky.windDirection - 90));
            var windDirection = new float3(0, 0, 30 * envEnt.meshmoonSky.windSpeed);
            windDirection = quat.Mul(windDirection);
            ent.rigidBody.ApplyTorque(windDirection);
        }
        else
            ent.rigidBody.ApplyTorque(new float3(0,0,30));

        this.createdLocalEnts.push(ent.id);
        return ent;
    },

    onUpdate : function(frametime)
    {
        var envEnt = scene.EntityByName(envEntName);
        if (envEnt == null || envEnt.meshmoonWater == null)
            return;

        for (var i = 0; i < this.waterAffectedEnts.length; i++)
        {
            var ent = this.waterAffectedEnts[i];
            if (ent == null || ent.placeable == null)
                continue;

            // Move hovering text to the correct position.
            var textEntity = scene.EntityByName(ent.name + "_text");
            if(textEntity != null)
            {
                var textPosition = ent.placeable.WorldPosition();
                textEntity.placeable.SetPosition(textPosition);
            }

            var floatingPos = ent.placeable.WorldPosition();
            var floatingHeight = 0.0;
            var queryPointCount = this.waterHeightQueryOffsets.length;
            for(var j = 0; j < queryPointCount; ++j)
            {
                var queryPos = ent.placeable.WorldPosition().Add(this.waterHeightQueryOffsets[j]);
                floatingHeight += envEnt.meshmoonWater.HeightAt(queryPos);
            }

            floatingPos.y = (floatingHeight / queryPointCount) + 1.8;
            ent.placeable.SetPosition(floatingPos);
        }
    }
});

var ShaderDemoServer = Class.extend(
{
    init: function()
    {
        this.lights = [];
        
        var lightEntities = scene.EntitiesWithComponent("DynamicComponent", "LightData");
        for(var i = 0; i < lightEntities.length; ++i)
        {
            var light = new MovingLight(lightEntities[i], 7 + (Math.random()*5));
            this.lights.push(light);
        }
        
        frame.Updated.connect(this, this.onUpdate);
    },
    
    onUpdate: function(frameTime)
    {
        for(var i = 0; i < this.lights.length; ++i)
            this.lights[i].update(frameTime);
    },
    
    shutDown: function()
    {
    }
});

function OnScriptDestroyed()
{
    if (_p != null)
        _p.shutDown();
}

if (IsClient())
{
    _p = new ShaderDemoClient();

    // Also run the server light moving if 
    // client is not connected to a server.
    // This will leak but is fine for local demoing.
    if (!rocket.IsConnectedToServer())
        new ShaderDemoServer();
}
else
    _p = new ShaderDemoServer();
